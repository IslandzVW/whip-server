#include "StdAfx.h"

#include "MeshQueryConnection.h"
#include "Settings.h"
#include "AppLog.h"

namespace iwintramesh
{

	MeshQueryConnection::MeshQueryConnection(boost::asio::io_service& ioService,
		const boost::asio::ip::tcp::endpoint& endPoint,
		boost::function<void()> disconnectCallback,
		boost::function<void(IntraMeshMsg::ptr)> dataRcvdCallback)
	: _meshQuerySocket(ioService), _endPoint(endPoint), _disconnectCallback(disconnectCallback),
		_messageRcvdCallback(dataRcvdCallback), _isConnected(false), _recvActive(false),
		_sendActive(false), _disconnectCallbackFired(false)
	{
		
	}


	MeshQueryConnection::~MeshQueryConnection()
	{
	}

	void MeshQueryConnection::connect(boost::function<void(const boost::system::error_code& error)> connectCallback)
	{
		_meshQuerySocket.async_connect(_endPoint,
			boost::bind(&MeshQueryConnection::onAfterMeshServiceConnect, this,
			  boost::asio::placeholders::error,
			  connectCallback));
	}

	void MeshQueryConnection::onAfterMeshServiceConnect(const boost::system::error_code& error,
		boost::function<void(const boost::system::error_code& error)> connectCallback)
	{
		if (! error) {
			SAFELOG(AppLog::instance().out() 
				<< "[IWINTRAMESH] Connection established to query service on intramesh server: "
				<< _endPoint
				<< std::endl);

			int BUF_SZ = whip::Settings::instance().get("tcp_bufsz").as<int>();

			_meshQuerySocket.set_option(boost::asio::socket_base::send_buffer_size(BUF_SZ));
			_meshQuerySocket.set_option(boost::asio::socket_base::receive_buffer_size(BUF_SZ));

			_isConnected = true;

			this->listenForMessage();

		} else {
			SAFELOG(AppLog::instance().error() 
				<< "[IWINTRAMESH] Unable to make connection to query service on intramesh server: "
				<< _endPoint
				<< std::endl);
			
			_isConnected = false;
		}

		connectCallback(error);
	}

	void MeshQueryConnection::listenForMessage()
	{
		_recvActive = true;

		IntraMeshMsg::ptr inboundMsg(new IntraMeshMsg());

		boost::asio::async_read(_meshQuerySocket,
			boost::asio::buffer(inboundMsg->getDataStorage(), IntraMeshMsg::MAX_PACKET), 
			boost::bind(&MeshQueryConnection::onAfterMeshMessageReceived, this,
			  boost::asio::placeholders::error,
			  boost::asio::placeholders::bytes_transferred,
			  inboundMsg));
	}

	void MeshQueryConnection::onAfterMeshMessageReceived(const boost::system::error_code& error,
		size_t bytesRcvd, IntraMeshMsg::ptr inboundMsg)
	{
		_recvActive = false;

		if (! error && bytesRcvd > 0) {
			_messageRcvdCallback(inboundMsg);
			this->listenForMessage();

		} else {
			this->printIoError(__FUNCTION__, error);
			this->close();
		}
	}
	
	void MeshQueryConnection::search(const std::string& uuid)
	{
		if (! _isConnected) {
			return;
		}

		if (! _sendActive) {
			this->sendQuery(uuid);

		} else {
			_sendQueue.push(uuid);
		}
	}

	void MeshQueryConnection::sendQuery(const std::string& uuid) 
	{
		_sendActive = true;

		IntraMeshMsg::ptr outboundMsg(new IntraMeshMsg(uuid));
		boost::asio::async_write(_meshQuerySocket,
			boost::asio::buffer(outboundMsg->getDataStorage(), IntraMeshMsg::MAX_PACKET), 
			boost::bind(&MeshQueryConnection::onAfterMeshMessageWritten, this,
			  boost::asio::placeholders::error,
			  boost::asio::placeholders::bytes_transferred,
			  outboundMsg));
	}

	void MeshQueryConnection::writeNextMessage()
	{
		if (! _sendQueue.empty()) {
			const std::string& uuid = _sendQueue.front();

			this->sendQuery(uuid);

			_sendQueue.pop();
		}
	}

	void MeshQueryConnection::onAfterMeshMessageWritten(const boost::system::error_code& error,
		size_t bytesSent, IntraMeshMsg::ptr inboundMsg)
	{
		_sendActive = false;

		if (! error && bytesSent > 0) {
			this->writeNextMessage();

		} else {
			this->printIoError(__FUNCTION__, error);
			this->close();
		}
	}

	void MeshQueryConnection::printIoError(const std::string& method, const boost::system::error_code& error)
	{
		SAFELOG(AppLog::instance().error() 
			<< "[IWINTRAMESH] ioerror " << method << ": " << error.message() << std::endl);
	}

	bool MeshQueryConnection::isConnected() const
	{
		return _isConnected;
	}

	void MeshQueryConnection::close()
	{
		if (_isConnected) {
			_meshQuerySocket.close();
			_isConnected = false;
		}

		if (!_recvActive && !_sendActive && !_disconnectCallbackFired) {
			_disconnectCallbackFired = true;
			_disconnectCallback();
		}
	}

}