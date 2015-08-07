#include "StdAfx.h"

#include "IntraMeshQueryClient.h"
#include "IntraMeshService.h"

namespace iwintramesh
{
	IntraMeshQueryClient::IntraMeshQueryClient(boost::asio::io_service& ioService,
		IntraMeshService& meshService, IAsyncStorageBackend::ptr diskBackend,
		bool debugging)
	: _ioService(ioService), _meshService(meshService), _diskBackend(diskBackend), 
		_conn(ioService), _debugging(debugging), _sendActive(false), _recvActive(false)
	{

	}


	IntraMeshQueryClient::~IntraMeshQueryClient()
	{
	}

	boost::asio::ip::tcp::socket& IntraMeshQueryClient::getConn()
	{
		return _conn;
	}

	void IntraMeshQueryClient::start()
	{
		_connectedEp = _conn.remote_endpoint();

		SAFELOG(AppLog::instance().out() << "[IWINTRAMESH] Accepted query connection from server: " << _connectedEp << std::endl);

		int BUF_SZ = whip::Settings::instance().get("tcp_bufsz").as<int>();

		_conn.set_option(boost::asio::socket_base::send_buffer_size(BUF_SZ));
		_conn.set_option(boost::asio::socket_base::receive_buffer_size(BUF_SZ));

		this->listenForMeshMessage();
	}

	const boost::asio::ip::tcp::endpoint& IntraMeshQueryClient::getEndpoint() const
	{
		return _connectedEp;
	}

	void IntraMeshQueryClient::listenForMeshMessage()
	{
		IntraMeshMsg::ptr inboundMsg(new IntraMeshMsg());

		_recvActive = true;

		boost::asio::async_read(_conn,
			boost::asio::buffer(inboundMsg->getDataStorage(), IntraMeshMsg::MAX_PACKET), 
			boost::bind(&IntraMeshQueryClient::onMeshMessageReceived, this,
			  boost::asio::placeholders::error,
			  boost::asio::placeholders::bytes_transferred,
			  inboundMsg));
	}

	void IntraMeshQueryClient::printIoError(const std::string& method, const boost::system::error_code& error)
	{
		SAFELOG(AppLog::instance().error() 
			<< "[IWINTRAMESH] ioerror " << method << ": " << error.message() << std::endl);
	}

	void IntraMeshQueryClient::onMeshMessageReceived(const boost::system::error_code& error,
		size_t bytesRcvd, IntraMeshMsg::ptr inboundMsg)
	{
		_recvActive = false;

		if (! error && bytesRcvd > 0) {
			this->processIntrameshMessage(inboundMsg);

		} else {
			this->printIoError(__FUNCTION__, error);
			this->close();
		}
	}

	void IntraMeshQueryClient::processIntrameshMessage(IntraMeshMsg::ptr inboundMsg)
	{
		if (inboundMsg->validate()) {
			_meshService.addMeshRequestStat();

			switch (inboundMsg->getRequestType())
			{
			case IntraMeshMsg::MESH_QUERY:
				this->processMeshQuery(inboundMsg);
				break;

			default:
				SAFELOG(AppLog::instance().error() 
					<< "[IWINTRAMESH] " << __FUNCTION__ 
					<< ": Unexpected message: Type: " << inboundMsg->getRequestType() << " "
					<< "From: " << _connectedEp << " "
					<< "Size: " << inboundMsg->getDataStorage().size()
					<< std::endl);

				this->close();
			}

		} else {
			SAFELOG(AppLog::instance().error() 
				<< "[IWINTRAMESH] " << __FUNCTION__ 
				<< ": Message was invalid: Type: " << inboundMsg->getRequestType() << " "
				<< "From: " << _connectedEp << " "
				<< "Size: " << inboundMsg->getDataStorage().size()
				<< std::endl);

			this->close();
		}
	}

	void IntraMeshQueryClient::processMeshQuery(IntraMeshMsg::ptr inboundMsg)
	{
		if (_debugging) {
			SAFELOG(AppLog::instance().out() << "[IWINTRAMESH] Received MESH_QUERY for " 
				<< inboundMsg->getAssetUUID() 
				<< " from "
				<< _connectedEp
				<< std::endl);
		}

		try {
			bool assetFound = _diskBackend->assetExists(inboundMsg->getAssetUUID());
			if (assetFound) {
				this->sendAssetFound(inboundMsg);
			} else {
				this->sendAssetNotFound(inboundMsg);
			}

		} catch (const std::exception& e) {
			SAFELOG(AppLog::instance().error() << "[IWINTRAMESH] Error while processing MESH_QUERY for " 
				<< inboundMsg->getAssetUUID() 
				<< " from " 
				<< _connectedEp
				<< ": "
				<< e.what()
				<< std::endl);

			this->sendAssetError(inboundMsg);
		}
	}

	void IntraMeshQueryClient::sendAssetFound(IntraMeshMsg::ptr inboundMsg)
	{
		if (_debugging) {
			SAFELOG(AppLog::instance().out() << "[IWINTRAMESH] Asset found for " 
				<< inboundMsg->getAssetUUID() 
				<< std::endl);
		}

		//stats
		_meshService.addPositiveMeshRequestStat();

		IntraMeshMsg::ptr outbound(new IntraMeshMsg(inboundMsg->getAssetUUID(), IntraMeshMsg::QR_FOUND));
		this->sendQueryResponse(outbound);
	}

	void IntraMeshQueryClient::sendAssetNotFound(IntraMeshMsg::ptr inboundMsg)
	{
		if (_debugging) {
			SAFELOG(AppLog::instance().out() << "[IWINTRAMESH] Asset not found for " 
				<< inboundMsg->getAssetUUID() 
				<< std::endl);
		}

		IntraMeshMsg::ptr outbound(new IntraMeshMsg(inboundMsg->getAssetUUID(), IntraMeshMsg::QR_NOTFOUND));
		this->sendQueryResponse(outbound);
	}

	void IntraMeshQueryClient::sendAssetError(IntraMeshMsg::ptr inboundMsg)
	{
		if (_debugging) {
			SAFELOG(AppLog::instance().out() << "[IWINTRAMESH] Error checking for " 
				<< inboundMsg->getAssetUUID() 
				<< std::endl);
		}

		IntraMeshMsg::ptr outbound(new IntraMeshMsg(inboundMsg->getAssetUUID(), IntraMeshMsg::QR_ERROR));
		this->sendQueryResponse(outbound);
	}

	bool IntraMeshQueryClient::connectionActive()
	{
		return _sendActive || _recvActive;
	}

	void IntraMeshQueryClient::close()
	{
		_conn.close();

		if (! this->connectionActive()) {
			_meshService.queryClientDisconnected(shared_from_this());
		}
	}

	void IntraMeshQueryClient::sendQueryResponse(IntraMeshMsg::ptr outbound)
	{
		if (! _sendActive) {
			_sendActive = true;

			_conn.async_send(
				boost::asio::buffer(outbound->getDataStorage(), 
				outbound->getDataStorage().size()), 
				boost::bind(&IntraMeshQueryClient::onAfterSendQueryResponse, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred,
				outbound));

		} else {
			IQCSendItem item;
			item.message = outbound;
			item.callBack = 
				boost::bind(&IntraMeshQueryClient::onAfterSendQueryResponse, this, 
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred,
					outbound);

			_pendingSends.push(item);
		}
	}

	void IntraMeshQueryClient::onAfterSendQueryResponse(const boost::system::error_code& error,
		size_t bytesSent, IntraMeshMsg::ptr outbound)
	{
		_sendActive = false;

		if (!error && bytesSent > 0) {
			this->listenForMeshMessage();
			this->processQueuedSend();

		} else {
			this->printIoError(__FUNCTION__, error);
			this->close();
		}
	}

	void IntraMeshQueryClient::processQueuedSend()
	{
		if (! _pendingSends.empty()) {
			IQCSendItem& item = _pendingSends.front();

			_sendActive = true;

			_conn.async_send(
				boost::asio::buffer(item.message->getDataStorage(), 
					item.message->getDataStorage().size()), 
				item.callBack
			);

			_pendingSends.pop();
		}
	}

	void IntraMeshQueryClient::sendHeartbeat(int flags)
	{
		IntraMeshMsg::ptr outbound(new IntraMeshMsg(flags));

		if (! _sendActive) {
			_sendActive = true;

			_conn.async_send(
			  boost::asio::buffer(outbound->getDataStorage(), 
				outbound->getDataStorage().size()), 
			  boost::bind(&IntraMeshQueryClient::onAfterSendHeartbeat, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred,
				outbound));

		} else {
			IQCSendItem item;
			item.message = outbound;
			item.callBack = 
				boost::bind(&IntraMeshQueryClient::onAfterSendHeartbeat, this, 
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred,
					outbound);

			_pendingSends.push(item);
		}
	}

	void IntraMeshQueryClient::onAfterSendHeartbeat(const boost::system::error_code& error,
		size_t bytesSent, IntraMeshMsg::ptr outbound)
	{
		_sendActive = false;

		if (!error && bytesSent > 0) {
			this->processQueuedSend();

		} else {
			this->printIoError(__FUNCTION__, error);
			this->close();
		}
	}
}