#include "StdAfx.h"
#include "RemoteAssetService.h"

#include "Settings.h"

#include <boost/array.hpp>


RemoteAssetService::RemoteAssetService(const boost::asio::ip::tcp::endpoint& ep,
	boost::asio::io_service& ioService)
	: _endpoint(ep), _ioService(ioService), _socket(_ioService), _assetConnectionEstablished(false),
	_sendActive(false), _recvActive(false)
{

}

RemoteAssetService::~RemoteAssetService()
{
}

void RemoteAssetService::setSafekillCallback(SafeKillCallback callback)
{
	_safeKillCallback = callback;
}

void RemoteAssetService::connect(RASConnectCallback connectCallback)
{
	_connectCallback = connectCallback;
	_socket.async_connect(_endpoint,
			boost::bind(&RemoteAssetService::onConnect, this,
			  boost::asio::placeholders::error));
}

void RemoteAssetService::onConnect(const boost::system::error_code& error)
{
	if (! error) {
		SAFELOG(AppLog::instance().out() 
			<< "[RAS] Connection established to asset service on server: "
			<< _endpoint
			<< std::endl);

		int BUF_SZ = whip::Settings::instance().get("tcp_bufsz").as<int>();

		_socket.set_option(boost::asio::socket_base::send_buffer_size(BUF_SZ));
		_socket.set_option(boost::asio::socket_base::receive_buffer_size(BUF_SZ));

		//begin authentication, wait for receive challenge
		_inboundAuthChallenge.reset(new AuthChallengeMsg(true));

		boost::asio::async_read(_socket,
			boost::asio::buffer(_inboundAuthChallenge->getDataStorage(), AuthChallengeMsg::MESSAGE_SIZE),
			boost::bind(
				&RemoteAssetService::onRecvChallenge, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));

	} else {
		SAFELOG(AppLog::instance().error() 
			<< "[RAS] Unable to make connection to asset service on intramesh server: "
			<< _endpoint
			<< std::endl);
			
		_assetConnectionEstablished = false;
		this->onConnectionResult();
	}
}

void RemoteAssetService::onConnectionResult()
{
	_connectCallback(shared_from_this(), _assetConnectionEstablished);
	if (_assetConnectionEstablished) {
		listenForServerResponses();
	}
}

void RemoteAssetService::onRecvChallenge(const boost::system::error_code& error, size_t bytesRcvd)
{
	if (!error && bytesRcvd != 0) {
		//generate a response to the challenge
		this->sendChallengeResponse();

	} else {
		SAFELOG(AppLog::instance().error() 
			<< "[RAS] Unable to authenticate with asset service on replication partner: "
			<< _endpoint
			<< std::endl);
			
		_assetConnectionEstablished = false;
		this->onConnectionResult();
	}
}

void RemoteAssetService::sendChallengeResponse()
{
	AuthResponseMsg::ptr respMessage(new AuthResponseMsg(_inboundAuthChallenge->getPhrase(), 
		whip::Settings::instance().get("password").as<std::string>()));

	boost::asio::async_write(_socket,
		boost::asio::buffer(respMessage->getDataStorage(), AuthResponseMsg::MESSAGE_SIZE),
		boost::bind(&RemoteAssetService::onChallengeResponseWrite, this,
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred,
			respMessage));
}

void RemoteAssetService::onChallengeResponseWrite(const boost::system::error_code& error, size_t bytesSent,
			AuthResponseMsg::ptr response)
{
	if (!error && bytesSent != 0) {
		AuthStatusMsg::ptr authStatus(new AuthStatusMsg());

		//read the response back from the server
		boost::asio::async_read(_socket,
		boost::asio::buffer(authStatus->getDataStorage(), AuthStatusMsg::MESSAGE_SIZE),
			boost::bind(&RemoteAssetService::onAuthenticationStatus, this,
				boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred,
				authStatus));

	} else {
		SAFELOG(AppLog::instance().error() 
			<< "[RAS] Unable to authenticate with asset service on peer server: "
			<< _endpoint
			<< ": "
			<< error.message()
			<< std::endl);
			
		_assetConnectionEstablished = false;
		this->onConnectionResult();
	}
}

void RemoteAssetService::onAuthenticationStatus(const boost::system::error_code& error, size_t bytesRcvd,
			AuthStatusMsg::ptr authStatus)
	{
		if (!error && bytesRcvd > 0) {
			if (! authStatus->validate()) {
				SAFELOG(AppLog::instance().error() 
					<< "[RAS] Unable to authenticate with asset service on intramesh server: "
					<< _endpoint
					<< ": The server's authentication response was invalid"
					<< std::endl);
			
				_assetConnectionEstablished = false;
				this->onConnectionResult();
				return;
			}

			if (authStatus->getAuthStatus() != AS_AUTH_SUCCESS) {
				SAFELOG(AppLog::instance().error() 
					<< "[RAS] Unable to authenticate with asset service on intramesh server: "
					<< _endpoint
					<< ": Invalid credentials"
					<< std::endl);
			
				_assetConnectionEstablished = false;
				this->onConnectionResult();
				return;
			}

			_assetConnectionEstablished = true;
			this->onConnectionResult();

		} else {
			SAFELOG(AppLog::instance().error() 
				<< "[RAS] Unable to authenticate with asset service on intramesh server: "
				<< _endpoint
				<< ": "
				<< error.message()
				<< std::endl);
			
			_assetConnectionEstablished = false;
			this->onConnectionResult();
		}
	}

void RemoteAssetService::getAsset(const std::string& assetId, bool cache, RASResponseCallback getAssetCallback)
{
	if (! _assetConnectionEstablished) {
		_ioService.post(boost::bind(getAssetCallback, Asset::ptr(), RASError(true, false), "Connection not established"));
	}

	if (!_sendActive) {
		this->sendGetRequest(assetId, getAssetCallback, cache);

	} else {
		//we already have a transfer in progress, store this transfer data for 
		ClientRequestMsg::RequestType type;

		if (cache) {
			type = ClientRequestMsg::RT_GET;
		} else {
			type = ClientRequestMsg::RT_GET_DONTCACHE;
		}

		_transferRequestQueue.push(PendingTransferRequest(assetId, getAssetCallback, type));
	}
}

void RemoteAssetService::sendGetRequest(const std::string& uuid, RASResponseCallback callBack, bool cache)
{
	_sendActive = true;

	//send the request for the given asset to this server
	ClientRequestMsg::ptr request;
	
	if (cache) {
		request.reset(new ClientRequestMsg(ClientRequestMsg::RT_GET, uuid));
	} else {
		request.reset(new ClientRequestMsg(ClientRequestMsg::RT_GET_DONTCACHE, uuid));
	}

	boost::asio::async_write(_socket,
		boost::asio::buffer(request->getHeaderData(), request->getHeaderData().size()),
		boost::bind(&RemoteAssetService::onAfterWriteAssetRequest, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred,
				request));

	_assetCallbackQueue.push(PendingTransferRequest(uuid, callBack, request->getType()));
}

void RemoteAssetService::doNextQueuedRequest()
{
	PendingTransferRequest& req = _transferRequestQueue.front();

	switch (req.get<2>()) {
	case ClientRequestMsg::RT_GET:
		this->getAsset(req.get<0>(), true, req.get<1>());
		break;

	case ClientRequestMsg::RT_GET_DONTCACHE:
		this->getAsset(req.get<0>(), false, req.get<1>());
		break;

	case ClientRequestMsg::RT_PUT:
		this->putAsset(req.get<3>(), req.get<1>());
		break;

	case ClientRequestMsg::RT_STORED_ASSET_IDS_GET:
		this->getStoredAssetIDsWithPrefix(req.get<0>(), req.get<1>());
		break;
	}

	_transferRequestQueue.pop();
}

void RemoteAssetService::onAfterWriteAssetRequest(const boost::system::error_code& error, size_t bytesSent,
		ClientRequestMsg::ptr outbound)
{
	_sendActive = false;

	if (!error && bytesSent > 0) {
		if (_assetConnectionEstablished && !_transferRequestQueue.empty()) {
			this->doNextQueuedRequest();
		}

		checkSafeKill();

	} else {
		SAFELOG(AppLog::instance().error() 
			<< "[RAS] Error while writing asset request to peer server "
			<< _endpoint
			<< ": "
			<< error.message()
			<< std::endl);

		this->close();
	}
}

void RemoteAssetService::close()
{
	_socket.close();
	_assetConnectionEstablished = false;

	//kill all transfer requests
	while (_transferRequestQueue.size() > 0) {
		PendingTransferRequest& req(_transferRequestQueue.front());

		SAFELOG(AppLog::instance().error() 
			<< "[RAS] Canceling transfer request " << req.get<0>()
			<< " due to peer server socket closure"
			<< std::endl);

		(req.get<1>())(Asset::ptr(), RASError(true, true), "Connection was terminated");

		//remember after this line req is invalid!
		_transferRequestQueue.pop();
	}

	//kill all sent callbacks waiting
	while (_assetCallbackQueue.size() > 0) {
		PendingTransferRequest& req(_assetCallbackQueue.front());

		SAFELOG(AppLog::instance().error() 
			<< "[RAS] Canceling callback for asset " << req.get<0>()
			<< " due to mesh server socket closure"
			<< std::endl);

		(req.get<1>())(Asset::ptr(), RASError(true, true), "Connection was terminated");

		//remember after this line req is invalid!
		_assetCallbackQueue.pop();
	}

	this->checkSafeKill();
}

void RemoteAssetService::checkSafeKill()
{
	if (_assetConnectionEstablished == false && _safeKillCallback) {
		if (!_recvActive && !_sendActive) {
			_safeKillCallback(shared_from_this());
		}
	}
}

void RemoteAssetService::listenForServerResponses()
{
	_recvActive = true;

	//read the response header
	ServerResponseMsg::ptr response(new ServerResponseMsg());
	boost::asio::async_read(_socket, 
		boost::asio::buffer(response->getHeader(), response->getHeader().size()),
		boost::bind(&RemoteAssetService::onReadResponseHeader, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred,
			response));
}

void RemoteAssetService::onReadResponseHeader(const boost::system::error_code& error, size_t bytesSent,
		ServerResponseMsg::ptr response)
{
	_recvActive = false;

	if (!error && bytesSent > 0) {
		//is the response valid and positive?
		if (response->validateHeader() && 
			response->getResponseCode() == ServerResponseMsg::RC_FOUND ||
			response->getResponseCode() == ServerResponseMsg::RC_OK) {
				
			//adjust data size
			response->initializeDataStorage();

			_recvActive = true;

#pragma warning (disable: 4503) //decorated name length exceeded
			//read the data
			boost::asio::async_read(_socket, 
				boost::asio::buffer(*(response->getData()), response->getData()->size()),
				boost::bind(&RemoteAssetService::onReadResponseData, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred,
					response));

		} else {
			//error, not found, or other condition
			SAFELOG(AppLog::instance().error() 
				<< "[RAS] Error from server: "
				<< _endpoint
				<< ", response code: " << (response->validateHeader() ? response->getResponseCode() : 0)
				<< std::endl);

			//invalid header disconnect this server
			if (! response->validateHeader()) {
				SAFELOG(AppLog::instance().error() 
					<< "[RAS] Received corrupted header from server "
					<< _endpoint << " forcing disconnect"
					<< std::endl);

				this->close();

			} else {
				//if there is data available in the form of an error code we have to read it
				//or it'll muck up our stream positioning
				if (response->getDataSize() > 0) {
					//adjust data size
					response->initializeDataStorage();

					_recvActive = true;

					//read the data and discard
					boost::asio::async_read(_socket, 
						boost::asio::buffer(*(response->getData()), response->getData()->size()),
						boost::bind(&RemoteAssetService::onHandleRequestErrorData, this,
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred,
							response));

				} else {
					// there is no data to read. We can safely start listening again
					SAFELOG(AppLog::instance().error() 
						<< "[IWINTRAMESH] No further error data is available "
						<< std::endl);

					//tell the waiter this transfer broke
					BOOST_ASSERT(! _assetCallbackQueue.empty());
					PendingTransferRequest& req = _assetCallbackQueue.front();
					req.get<1>()(Asset::ptr(), RASError(true, false), "");
					_assetCallbackQueue.pop();

					this->listenForServerResponses();
				}
			}

				
		}

			

	} else {
		SAFELOG(AppLog::instance().error() 
			<< "[IWINTRAMESH] Error while reading asset header from mesh server "
			<< _endpoint
			<< ": "
			<< error.message()
			<< std::endl);

		//close the connection something is broken
		this->close();
	}
}

void RemoteAssetService::onReadResponseData(const boost::system::error_code& error, size_t bytesSent,
	ServerResponseMsg::ptr response)
{
	_recvActive = false;

	if (!error && _assetConnectionEstablished) {
		//this asset should match the top requestor
		BOOST_ASSERT(! _assetCallbackQueue.empty());
		PendingTransferRequest& req = _assetCallbackQueue.front();

		switch (req.get<2>()) 
		{
		case ClientRequestMsg::RT_GET:
		case ClientRequestMsg::RT_GET_DONTCACHE:
			this->handleAssetReturned(req, response);
			break;

		case ClientRequestMsg::RT_MAINT_PURGELOCALS:
		case ClientRequestMsg::RT_PURGE:
		case ClientRequestMsg::RT_PUT:
			this->handleConfirmation(req, response);
			break;

		case ClientRequestMsg::RT_STATUS_GET:
		case ClientRequestMsg::RT_STORED_ASSET_IDS_GET:
			this->handleDataReturned(req, response);
			break;

		default:
			SAFELOG(AppLog::instance().error() 
				<< "[RAS] Unhandled request type: " 
				<< req.get<2>());

			this->close();
			return;
		}

		if (_assetCallbackQueue.size() > 0) {
			_assetCallbackQueue.pop();	
		}

	} else {
		SAFELOG(AppLog::instance().error() 
			<< "[RAS] Error while reading asset data from mesh server "
			<< _endpoint
			<< ": "
			<< error.message()
			<< std::endl);
			
		//close the connection something is broken
		this->close();
	}
}

void RemoteAssetService::handleConfirmation(PendingTransferRequest& req, ServerResponseMsg::ptr response)
{
	req.get<1>()(Asset::ptr(), RASError(false, false), "");
	this->listenForServerResponses();
}

void RemoteAssetService::handleAssetReturned(PendingTransferRequest& req, ServerResponseMsg::ptr response)
{
	Asset::ptr asset(new Asset(response->getData()));
	if (asset->getUUID() == req.get<0>()) {
		req.get<1>()(asset, RASError(false, false), "");
		this->listenForServerResponses();

	} else {
		//something is very wrong
		SAFELOG(AppLog::instance().error() 
			<< "[RAS] Asset data received does not match next waiting request "
			<< _endpoint
			<< " expected: " << req.get<0>()
			<< " got: " << asset->getUUID()
			<< std::endl);

		//close will also signal everyone waiting
		this->close();
	}
}

void RemoteAssetService::handleDataReturned(PendingTransferRequest& req, ServerResponseMsg::ptr response)
{
	std::string data;
	data.insert(data.begin(), response->getData()->begin(), response->getData()->end());
	req.get<1>()(Asset::ptr(), RASError(false, false), data);

	this->listenForServerResponses();
}

void RemoteAssetService::onHandleRequestErrorData(const boost::system::error_code& error, size_t bytesSent,
		ServerResponseMsg::ptr response)
{
	_recvActive = false;

	if (!error && bytesSent > 0) {
		std::string error;
		error.insert(error.begin(), response->getData()->begin(), response->getData()->end());

		SAFELOG(AppLog::instance().error() 
			<< "[RAS] Error follows: "
			<< "Endpoint: " << _endpoint << ", "
			<< "error msg: " << error
			<< std::endl);

		//tell the waiter this transfer broke
		BOOST_ASSERT(! _assetCallbackQueue.empty());
		PendingTransferRequest& req = _assetCallbackQueue.front();

		req.get<1>()(Asset::ptr(), RASError(true, false), error);
		_assetCallbackQueue.pop();

		this->listenForServerResponses();

	} else {
		this->close();
	}
}

void RemoteAssetService::putAsset(Asset::ptr asset,  RASResponseCallback putAssetCallback)
{
	if (! _assetConnectionEstablished) {
		_ioService.post(boost::bind(putAssetCallback, Asset::ptr(), RASError(true, false), "Connection not established"));
	}

	if (!_sendActive) {
		this->sendPutRequest(asset, putAssetCallback);

	} else {
		//we already have a transfer in progress, store this transfer data for later
		_transferRequestQueue.push(PendingTransferRequest(asset->getUUID(), putAssetCallback, ClientRequestMsg::RT_PUT, asset));
	}
}

void RemoteAssetService::sendPutRequest(Asset::ptr asset, RASResponseCallback putAssetCallback)
{
	_sendActive = true;

	//send the request for the given asset to this server
	ClientRequestMsg::ptr request(new ClientRequestMsg(asset));

	//set up scatter-write 
	boost::array<boost::asio::const_buffer, 2> bufs = {
		boost::asio::buffer(request->getHeaderData(), request->getHeaderData().size()),
		boost::asio::buffer(*request->getData(), request->getData()->size())
	};

	boost::asio::async_write(_socket, bufs,
		boost::bind(&RemoteAssetService::onAfterWriteAssetRequest, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred,
				request));

	_assetCallbackQueue.push(PendingTransferRequest(asset->getUUID(), putAssetCallback, request->getType()));
}

void RemoteAssetService::getStoredAssetIDsWithPrefix(const std::string& prefix, RASResponseCallback getStoredAssetIDsCallback)
{
	if (! _assetConnectionEstablished) {
		_ioService.post(boost::bind(getStoredAssetIDsCallback, Asset::ptr(), RASError(true, false), "Connection not established"));
	}

	if (!_sendActive) {
		this->sendAssetIdsRequest(prefix, getStoredAssetIDsCallback);

	} else {
		//we already have a transfer in progress, store this transfer data for later
		_transferRequestQueue.push(PendingTransferRequest(prefix, getStoredAssetIDsCallback, ClientRequestMsg::RT_STORED_ASSET_IDS_GET));
	}
}

void RemoteAssetService::sendAssetIdsRequest(const std::string& prefix, RASResponseCallback getStoredAssetIDsCallback)
{
	_sendActive = true;
	
	std::string fixedPfx(prefix);

	if (prefix.length() != 32) {
		int ldiff = 32 - (int)prefix.length();
		std::string fill(ldiff, '0');
		fixedPfx += fill;
	}

	//send the request for the given asset to this server
	ClientRequestMsg::ptr request(new ClientRequestMsg(ClientRequestMsg::RT_STORED_ASSET_IDS_GET, fixedPfx));

	boost::asio::async_write(_socket,
		boost::asio::buffer(request->getHeaderData(), request->getHeaderData().size()),
		boost::bind(&RemoteAssetService::onAfterWriteAssetRequest, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred,
				request));

	_assetCallbackQueue.push(PendingTransferRequest(prefix, getStoredAssetIDsCallback, ClientRequestMsg::RT_STORED_ASSET_IDS_GET));
}