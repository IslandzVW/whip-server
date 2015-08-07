#include "StdAfx.h"

#include "CSAuthenticated.h"
#include "AssetClient.h"
#include "RequestError.h"
#include "Validator.h"
#include "AssetServer.h"

#include <boost/bind.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <sstream>

namespace pt = boost::posix_time;

/*whip::ObjectPool<ServerResponseMsg::ptr, ServerResponseMsgCreator> CSAuthenticated::s_ResponsePool
	= whip::ObjectPool<ServerResponseMsg::ptr, ServerResponseMsgCreator>(ServerResponseMsgCreator());*/

CSAuthenticated::CSAuthenticated(boost::asio::io_service& ioService, IAsyncStorageBackend::ptr storage,
								 iwintramesh::MeshStorageBackend::ptr meshStorage,
								 bool isMeshConnection)
: _ioService(ioService), _storage(storage), _meshStorage(meshStorage), _isMeshConnection(isMeshConnection),
	_isActive(false), _activeSince(pt::second_clock::local_time())
{
}

CSAuthenticated::~CSAuthenticated()
{
}

void CSAuthenticated::setClient(AssetClientPtr client)
{
	_client = client;
}

void CSAuthenticated::waitForRequest()
{
	_currentRequest.reset(new ClientRequestMsg());
	_isActive = false;

	AssetClient::ptr client;
	if (client = _client.lock()) {
		boost::asio::async_read(client->getConn(),
			boost::asio::buffer(_currentRequest->getHeaderData(), ClientRequestMsg::HEADER_SIZE),
			boost::bind(
			  &CSAuthenticated::handleClientRequestHeaderRead, shared_from_this(),
				boost::asio::placeholders::error));
	}
}

void CSAuthenticated::handleClientRequestHeaderRead(const boost::system::error_code& error)
{
	if (! error) {
		//we now have a complete header, read it and figure out what type of message we're dealing with
		try {
			//if were here we got a request. Update stats
			AssetClient::AddRequestToStats();

			_isActive = true;
			_activeSince = pt::second_clock::local_time();

			switch (_currentRequest->getType()) {
				case ClientRequestMsg::RT_GET:
					this->handleGetRequest(true);
					break;

				case ClientRequestMsg::RT_PURGE:
					this->handlePurgeRequest();
					break;

				case ClientRequestMsg::RT_PUT:
					this->handlePutRequest();
					break;

				case ClientRequestMsg::RT_TEST:
					this->handleTestRequest();
					break;

				case ClientRequestMsg::RT_MAINT_PURGELOCALS:
					this->handlePurgeLocals();
					break;

				case ClientRequestMsg::RT_STATUS_GET:
					this->handleStatusGet();
					break;

				case ClientRequestMsg::RT_STORED_ASSET_IDS_GET:
					this->handleStoredAssetIdsGet();
					break;

				case ClientRequestMsg::RT_GET_DONTCACHE:
					this->handleGetRequest(false);
					break;

				default:
					throw RequestError("Invalid request type sent to server");
			}

		} catch (const std::exception& e) {
			SAFELOG(AppLog::instance().out() << "handleClientRequestHeaderRead(): Error while processing header: " 
				<< e.what() << std::endl);

			AssetClient::ptr client;
			if (client = _client.lock()) {
				client->disconnect();
			}
		}

	} else {
		this->handleIoFailure(__FUNCTION__, error);

	}
}

void CSAuthenticated::handleStatusGet()
{
	_statusStream.reset(new std::stringstream());
	(*_statusStream) << "WHIP Server Status" << std::endl << std::endl;

	if (AssetClient::ptr client = _client.lock()) {
		AssetServer& server(client->getParentServer());
		server.getStatus(_statusStream);

		_storage->getStatus(_statusStream, boost::bind(&CSAuthenticated::statusGetCompleted, this));
	}
}

void CSAuthenticated::statusGetCompleted()
{
	_meshStorage->getStatus(_statusStream);

	ServerResponseMsg::ptr currentResponse(
		new ServerResponseMsg(ServerResponseMsg::RC_OK, "00000000000000000000000000000000", _statusStream->str()));

	_statusStream.reset();

	this->sendResponse(currentResponse);
}

void CSAuthenticated::handleStoredAssetIdsGet()
{
	_assetIdStream.reset(new std::stringstream());

	if (AssetClient::ptr client = _client.lock()) {
		AssetServer& server(client->getParentServer());
		
		std::string uuidStr(_currentRequest->getUUID());
		std::string prefix(uuidStr.substr(0, 3));
		_storage->getStoredAssetIDsWithPrefix(prefix, _assetIdStream, boost::bind(&CSAuthenticated::storedAssetIdsGetCompleted, this));
	}
}

void CSAuthenticated::storedAssetIdsGetCompleted()
{
	ServerResponseMsg::ptr currentResponse(
		new ServerResponseMsg(ServerResponseMsg::RC_OK, "00000000000000000000000000000000", _assetIdStream->str()));

	_assetIdStream.reset();

	this->sendResponse(currentResponse);
}

void CSAuthenticated::handleIoFailure(const std::string& method, const boost::system::error_code& error)
{
	AssetClient::ptr client;
	if (client = _client.lock()) {
		client->disconnect();
		SAFELOG(AppLog::instance().out() << method << ": Connection failed " << error.message() << std::endl);
	} else {
		SAFELOG(AppLog::instance().error() << method << ": Client instance disappeared" << std::endl);
	}
}

void CSAuthenticated::handleGetRequest(bool cacheResult)
{
	try {
		//validate asset UUID
		if (! Validator::IsValidUUID(_currentRequest->getUUID())) {
			//invalid asset id
			throw std::runtime_error("Invalid asset UUID: " + _currentRequest->getUUID());
		}
		
		unsigned int flags = SF_NONE;
		if (! cacheResult) {
			flags = SF_DONTCACHE;
		}

		_storage->getAsset(_currentRequest->getUUID(), flags,
			boost::bind(&CSAuthenticated::handleStorageGetResponse, this, _1, _2, _3));

	} catch (const std::exception& e) {
		SAFELOG(AppLog::instance().out() << __FUNCTION__ << ":  Unable to fetch asset: " << e.what() << std::endl);
		
		//send the error to the client
		this->sendError(e.what());
	}
}

void CSAuthenticated::handleStorageGetResponse(Asset::ptr foundAsset, bool success, AssetStorageError::ptr error)
{
	if (success) {
		//start working with a new response message
		ServerResponseMsg::ptr currentResponse(new ServerResponseMsg(ServerResponseMsg::RC_FOUND, foundAsset->getUUID(), foundAsset->getData()));
		
		//set the response to the client
		this->sendResponse(currentResponse);

	} else {

		if (error && error->isCritical()) {
			SAFELOG(AppLog::instance().error() << __FUNCTION__ << ":  Asset read failed: " << error->what() << std::endl);
		}

		if (! _isMeshConnection) {
			//try intramesh
			_meshStorage->getAsset(_currentRequest->getUUID(), boost::bind(&CSAuthenticated::handleIntrameshGetResponse, this, _1, _2));

		} else {
			//if this is a mesh connection and lookup fails for any reason we do NOT retry intramesh
			ServerResponseMsg::ptr currentResponse(new ServerResponseMsg(ServerResponseMsg::RC_NOTFOUND, _currentRequest->getUUID()));
			
			//set the response to the client
			this->sendResponse(currentResponse);
		}
	}
}

void CSAuthenticated::handleIntrameshGetResponse(std::string uuid, Asset::ptr foundAsset)
{
	if (foundAsset) {
		//start working with a new response message
		ServerResponseMsg::ptr currentResponse(new ServerResponseMsg(ServerResponseMsg::RC_FOUND, foundAsset->getUUID(), foundAsset->getData()));
		
		//set the response to the client
		this->sendResponse(currentResponse);

	} else {
		//start working with a new response message
		ServerResponseMsg::ptr currentResponse(new ServerResponseMsg(ServerResponseMsg::RC_NOTFOUND, uuid));
		
		//set the response to the client
		this->sendResponse(currentResponse);
	}
}

void CSAuthenticated::sendError(const std::string& errorMsg)
{
	ServerResponseMsg::ptr response(new ServerResponseMsg(ServerResponseMsg::RC_ERROR, _currentRequest->getUUID(), errorMsg));

	//set the error to the client
	this->sendResponse(response);
}

void CSAuthenticated::sendResponse(ServerResponseMsg::ptr response)
{
	AssetClient::ptr client;
	if (client = _client.lock()) {
		//send the asset header
		boost::asio::async_write(client->getConn(),
			boost::asio::buffer(response->getHeader(), response->getHeader().size()),
			boost::bind(&CSAuthenticated::handleResponseHeaderWrite, shared_from_this(),
				  boost::asio::placeholders::error, response));

	}
}

void CSAuthenticated::handleResponseHeaderWrite(const boost::system::error_code& error, 
												ServerResponseMsg::ptr response)
{
	if (! error) {
		//no error, we can write the packet data now, if any

		//any data?
		if (response->getData()) {

			AssetClient::ptr client;
			if (client = _client.lock()) {
				//yes, send the contents and update stats
				whip::byte_array_ptr data(response->getData());
				boost::asio::async_write(client->getConn(),
					boost::asio::buffer(*data, data->size()),
					boost::bind(&CSAuthenticated::handleResponseDataWrite, shared_from_this(),
						  boost::asio::placeholders::error, response));

				AssetClient::AddTransferToStats(boost::numeric_cast<int>(data->size()));

				if (_isMeshConnection) {
					AssetClient::AddMeshTransferToStats(boost::numeric_cast<int>(data->size()));
				}
			}

		} else {
			//no, start waiting for another request

			this->waitForRequest();

		}

	} else {
		this->handleIoFailure(__FUNCTION__, error);
	}
}

void CSAuthenticated::handleResponseDataWrite(const boost::system::error_code& error,
											  ServerResponseMsg::ptr response)
{
	if (! error) {
		//data write complete, clear the response and wait for next request
		this->waitForRequest();

	} else {
		//we had an error
		this->handleIoFailure(__FUNCTION__, error);

	}
}

void CSAuthenticated::handleStoragePurgeResponse(bool success, AssetStorageError::ptr error)
{
	if (success) {
		//start working with a new response message
		ServerResponseMsg::ptr response(new ServerResponseMsg(ServerResponseMsg::RC_OK, _currentRequest->getUUID()));

		//set the response to the client
		this->sendResponse(response);
	} else {
		//send the error to the client
		this->sendError(error->what());
	}
}

void CSAuthenticated::handlePurgeRequest()
{
	try {
		//validate asset UUID
		if (! Validator::IsValidUUID(_currentRequest->getUUID())) {
			//invalid asset id
			throw std::runtime_error("Invalid asset UUID: " + _currentRequest->getUUID());
		}

		//ask storage to purge the asset
		_storage->purgeAsset(_currentRequest->getUUID(), 
			boost::bind(&CSAuthenticated::handleStoragePurgeResponse, this, _1, _2));
		

	} catch (const std::exception& e) {
		SAFELOG(AppLog::instance().out() << __FUNCTION__ << ":  Unable to purge asset: " << e.what() << std::endl);
		
		//send the error to the client
		this->sendError(e.what());
	}
}

void CSAuthenticated::handlePutRequest()
{
	//init the data section of our request
	_currentRequest->initDataStorageFromHeader();

	AssetClient::ptr client;
	if (client = _client.lock()) {
		//read the rest of the incoming data
		boost::asio::async_read(client->getConn(),
			boost::asio::buffer(*_currentRequest->getData(), _currentRequest->getDataSize()),
			boost::bind(
			  &CSAuthenticated::handleAssetDataRead, shared_from_this(),
				boost::asio::placeholders::error));
	}
}

void CSAuthenticated::handleStorageWriteResponse(bool success, AssetStorageError::ptr error)
{
	if (success) {
		//start working with a new response message
		ServerResponseMsg::ptr response(new ServerResponseMsg(ServerResponseMsg::RC_OK, _currentRequest->getUUID()));
		
		//set the response to the client
		this->sendResponse(response);

	} else {

		if (error && error->isCritical()) {
			SAFELOG(AppLog::instance().error() << __FUNCTION__ << ":  Asset write failed: " << error->what() << std::endl);
		}

		this->sendError(error->what());

	}

	//update asset stats
	AssetClient::AddTransferToStats(_currentRequest->getDataSize());
}

void CSAuthenticated::handleAssetDataRead(const boost::system::error_code& error)
{
	if (! error) {
		try {
			//create an asset from the data given 
			Asset::ptr newAsset(new Asset(_currentRequest->getData()));

			//validate asset UUID
			if (! Validator::IsValidUUID(newAsset->getUUID())) {
				//invalid asset id
				throw std::runtime_error("Invalid asset UUID: " + _currentRequest->getUUID());
			}

			//ask storage to save the asset
			_storage->storeAsset(newAsset, boost::bind(&CSAuthenticated::handleStorageWriteResponse, this, _1, _2));

		} catch (const std::exception& e) {
			SAFELOG(AppLog::instance().out() << __FUNCTION__ << ":  Unable to store asset: " << e.what() << std::endl);
			
			//send the error to the client
			this->sendError(e.what());
		}

	} else {
		this->handleIoFailure(__FUNCTION__, error);
	}
}

void CSAuthenticated::handleTestRequest()
{
	try {
		//validate asset UUID
		if (! Validator::IsValidUUID(_currentRequest->getUUID())) {
			//invalid asset id
			throw std::runtime_error("Invalid asset UUID: " + _currentRequest->getUUID());
		}

		//ask storage if we have this asset
		bool haveAsset = _storage->assetExists(_currentRequest->getUUID());
		
		ServerResponseMsg::ResponseCode responseCode;
		if (haveAsset) responseCode = ServerResponseMsg::RC_FOUND;
		else responseCode = ServerResponseMsg::RC_NOTFOUND;

		//start working with a new response message
		ServerResponseMsg::ptr response(new ServerResponseMsg(responseCode, _currentRequest->getUUID()));

		//set the response to the client
		this->sendResponse(response);

	} catch (const std::exception& e) {
		SAFELOG(AppLog::instance().out() << __FUNCTION__ << ":  Unable to test for asset: " << e.what() << std::endl);
		
		//send the error to the client
		this->sendError(e.what());
	}
}

void CSAuthenticated::handlePurgeLocals()
{
	try {
		_storage->beginPurgeLocals();

		//start working with a new response message
		ServerResponseMsg::ptr response(new ServerResponseMsg(ServerResponseMsg::RC_OK, "00000000000000000000000000000000"));

		//set the response to the client
		this->sendResponse(response);

	} catch (const std::exception& e) {
		SAFELOG(AppLog::instance().out() << __FUNCTION__ << ":  Unable to test for asset: " << e.what() << std::endl);
		
		//send the error to the client
		this->sendError(e.what());
	}
}

void CSAuthenticated::start()
{
	AssetClient::ptr client;
	if (client = _client.lock()) {
		SAFELOG(AppLog::instance().out() << "Client authentication successful > " << client->getConn().remote_endpoint().address().to_string() << std::endl);

		this->waitForRequest();
	}
}

std::string CSAuthenticated::getStateDescription()
{
	if (_isActive) {
		std::string status;
		status = "ACTIVE " + pt::to_simple_string(pt::second_clock::local_time() - _activeSince);
		
		if (_currentRequest) {
			if (_currentRequest->getType() == ClientRequestMsg::RT_GET) {
				status += " [GET " + _currentRequest->getUUID() + "]";

			} else if (_currentRequest->getType() == ClientRequestMsg::RT_PUT) {
				status += " [PUT " + _currentRequest->getUUID() + "]";

			}
		}

		return status;

	} else {
		return "IDLE " + pt::to_simple_string(pt::second_clock::local_time() - _activeSince);
	}
}