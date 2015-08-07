#pragma once

#include "AuthChallengeMsg.h"
#include "AuthResponseMsg.h"
#include "AuthStatusMsg.h"
#include "ClientRequestMsg.h"
#include "ServerResponseMsg.h"

#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/tuple/tuple.hpp>

#include <vector>
#include <string>
#include <queue>

//TODO: Much of this classes implementation came from MeshServer.cpp  As that code is throughly tested
//I dont yet want to replace the asset service portions of that class by using this class,
//however this will need to be done during the next round of work on WHIP as this class violates
//DRY

class RASError
{
private:
	void eval_function () {}
	typedef void (RASError:: * safe_bool_type) ();

	bool _isError;
	bool _isFatal;

public:
	RASError(bool isError, bool isFatal) : _isError(isError), _isFatal(isFatal)
	{

	}

	operator safe_bool_type () const
	{ 
		return _isError ? &RASError::eval_function : 0; 
	}

	bool isFatal() const
	{
		return _isFatal;
	}
};

/**
 * Represents an asset service running on another server
 */
class RemoteAssetService : public boost::enable_shared_from_this<RemoteAssetService>
{
public:
	typedef boost::shared_ptr<RemoteAssetService> ptr;

	typedef boost::function<void (RemoteAssetService::ptr, bool)> RASConnectCallback;
	typedef boost::function<void(Asset::ptr, RASError, std::string)> RASResponseCallback;
	typedef boost::function<void (RemoteAssetService::ptr)> SafeKillCallback;

private:
	boost::asio::ip::tcp::endpoint _endpoint;
	boost::asio::io_service& _ioService;

	boost::asio::ip::tcp::socket _socket;

	AuthChallengeMsg::ptr _inboundAuthChallenge;

	bool _assetConnectionEstablished;

	RASConnectCallback _connectCallback;

	bool _sendActive;
	bool _recvActive;

	typedef boost::tuples::tuple<std::string, RASResponseCallback, ClientRequestMsg::RequestType, Asset::ptr> PendingTransferRequest;
	typedef std::queue<PendingTransferRequest> TransferRequestQueue;
	TransferRequestQueue _transferRequestQueue;
	TransferRequestQueue _assetCallbackQueue;

	SafeKillCallback _safeKillCallback;


	void onConnect(const boost::system::error_code& error);
	void onConnectionResult();
	void onRecvChallenge(const boost::system::error_code& error, size_t bytesRcvd);
	void sendChallengeResponse();
	void onChallengeResponseWrite(const boost::system::error_code& error, size_t bytesSent,
			AuthResponseMsg::ptr response);
	void onAuthenticationStatus(const boost::system::error_code& error, size_t bytesRcvd,
			AuthStatusMsg::ptr authStatus);
	void sendGetRequest(const std::string& uuid, RASResponseCallback callBack, bool cache);
	void onAfterWriteAssetRequest(const boost::system::error_code& error, size_t bytesSent,
		ClientRequestMsg::ptr outbound);

	void checkSafeKill();

	void listenForServerResponses();
	void onReadResponseHeader(const boost::system::error_code& error, size_t bytesSent,
		ServerResponseMsg::ptr response);
	void onHandleRequestErrorData(const boost::system::error_code& error, size_t bytesSent,
		ServerResponseMsg::ptr response);
	void onReadResponseData(const boost::system::error_code& error, size_t bytesSent,
		ServerResponseMsg::ptr response);
	void handleAssetReturned(PendingTransferRequest& req, ServerResponseMsg::ptr response);
	void handleDataReturned(PendingTransferRequest& req, ServerResponseMsg::ptr response);
	void handleConfirmation(PendingTransferRequest& req, ServerResponseMsg::ptr response);

	void doNextQueuedRequest();

	void sendAssetIdsRequest(const std::string& prefix, RASResponseCallback getStoredAssetIDsCallback);

	void checkResumeNextReplRound();

	void sendPutRequest(Asset::ptr asset, RASResponseCallback putAssetCallback);

public:
	RemoteAssetService(const boost::asio::ip::tcp::endpoint& ep, boost::asio::io_service& ioService);
	virtual ~RemoteAssetService();

	/**
	 * Sets the callback that will be activated when this socket has disconnected and
	 * is in a state where it is ready to be destroyed
	 */
	void setSafekillCallback(SafeKillCallback callback);

	/**
	 * Connects to the server
	 */
	void connect(RASConnectCallback connectCallback);

	/**
	 * Closes the connection to the server
	 */
	void close();

	/**
	 * Retrieves an asset from the remote server
	 */
	void getAsset(const std::string& assetId, bool cache, RASResponseCallback getAssetCallback);

	/**
	 * Stores an asset to the remote server
	 */
	void putAsset(Asset::ptr asset,  RASResponseCallback putAssetCallback);

	/**
	 * Returns all asset IDs on the remote server that match the given 3 character hex prefix
	 */
	void getStoredAssetIDsWithPrefix(const std::string& prefix, RASResponseCallback getStoredAssetIDsCallback);
};

