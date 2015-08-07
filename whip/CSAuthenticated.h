#pragma once

#include "IConnectionState.h"
#include "IAsyncStorageBackend.h"
#include "ClientRequestMsg.h"
#include "ServerResponseMsg.h"
#include "MeshStorageBackend.h"
#include "AssetStorageError.h"

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <sstream>
#include <string>

class AssetClient;

/**
 * After the user pases authentication this connection state handles
 * communication with the client
*/
class CSAuthenticated 
	:	public IConnectionState,
		public boost::enable_shared_from_this<CSAuthenticated>
{
private:
	//static whip::ObjectPool<ServerResponseMsg::ptr, ServerResponseMsgCreator> s_ResponsePool;

	boost::asio::io_service& _ioService;
	IAsyncStorageBackend::ptr _storage;
	iwintramesh::MeshStorageBackend::ptr _meshStorage;
	boost::weak_ptr<AssetClient> _client;

	bool _isActive;
	boost::posix_time::ptime _activeSince;
	
	/**
	The current client request we're working with
	*/
	ClientRequestMsg::ptr _currentRequest;

	/**
	 * Whether or not this is an intramesh connection
	 */
	bool _isMeshConnection;

	/**
	 * When server status is requested, this is used to store the results
	 */
	boost::shared_ptr<std::stringstream> _statusStream;

	/**
	 * When asset id lists are requested, this is used to store the results
	 */
	boost::shared_ptr<std::stringstream> _assetIdStream;

	/**
	Handles client I/O failure.  Logs and disconnects the client
	*/
	void handleIoFailure(const std::string& method, const boost::system::error_code& error);

	/**
	Sends the current response to the client
	*/
	void sendResponse(ServerResponseMsg::ptr response);
	
	/**
	Sends an error to the client
	*/
	void sendError(const std::string& errorMsg);

	void handleStorageGetResponse(Asset::ptr foundAsset, bool success, AssetStorageError::ptr error);
	void handleStorageWriteResponse(bool success, AssetStorageError::ptr error);
	void handleStoragePurgeResponse(bool success, AssetStorageError::ptr error);

	void statusGetCompleted();
	void storedAssetIdsGetCompleted();

public:
	CSAuthenticated(boost::asio::io_service& ioService, IAsyncStorageBackend::ptr storage,
		iwintramesh::MeshStorageBackend::ptr meshStorage, bool isMeshConnection);
	virtual ~CSAuthenticated();

	virtual void setClient(AssetClientPtr client);
	virtual void start();
	virtual std::string getStateDescription();

	/**
	Waits to receive a new request from the client
	*/
	void waitForRequest();

	/**
	Handles after the read of the request header
	*/
	void handleClientRequestHeaderRead(const boost::system::error_code& error);

	/**
	Handles after the write of the response header
	*/
	void handleResponseHeaderWrite(const boost::system::error_code& error,
		ServerResponseMsg::ptr response);

	/**
	Handles after the write of the response data
	*/
	void handleResponseDataWrite(const boost::system::error_code& error,
		ServerResponseMsg::ptr response);

	/**
	Handles after the read of the asset data from a request
	*/
	void handleAssetDataRead(const boost::system::error_code& error);

	//handlers for the request types
	void handleGetRequest(bool cacheResult);
	void handlePurgeRequest();
	void handlePutRequest();
	void handleTestRequest();
	void handlePurgeLocals();
	void handleStatusGet();
	void handleStoredAssetIdsGet();

	/**
	 * Handles the get response from intramesh
	 */
	void handleIntrameshGetResponse(std::string uuid, Asset::ptr asset);
};
