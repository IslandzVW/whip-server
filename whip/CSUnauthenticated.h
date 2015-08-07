#pragma once

#include "IConnectionState.h"
#include "IAsyncStorageBackend.h"
#include "AuthChallengeMsg.h"
#include "AuthResponseMsg.h"
#include "AuthStatusMsg.h"
#include "MeshStorageBackend.h"

#include <boost/weak_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <string>

class AssetClient;

/**
Connection state before authentication.  Waits for the first packet to be an auth packet.
If it's not, disconnects the client.  If auth fails disconnect the client.
*/
class CSUnauthenticated 
	:	public IConnectionState, 
		public boost::enable_shared_from_this<CSUnauthenticated>
{
private:
	boost::asio::io_service& _ioService;
	boost::weak_ptr<AssetClient> _client;
	IAsyncStorageBackend::ptr _storage;
	iwintramesh::MeshStorageBackend::ptr _meshStorage;

	/**
		The challenge sent to the client
	*/
	AuthChallengeMsg::ptr _challenge;
	/**
		The response returned from the client
	*/
	AuthResponseMsg::ptr _response;
	/**
		The authstatus sent to the client
	*/
	AuthStatusMsg::ptr _authStatus;
	
	/**
	 * Whether or not this connection is to a server
	 */
	bool _isMeshConnection;

	/**
	Generates and sends the challenge to the client
	*/
	void sendAuthChallenge();



public:
	CSUnauthenticated(boost::asio::io_service& ioService, IAsyncStorageBackend::ptr storage,
		iwintramesh::MeshStorageBackend::ptr meshStorage);
	virtual ~CSUnauthenticated();

	virtual void setClient(AssetClientPtr client);

	virtual void start();

	virtual std::string getStateDescription();

	/**
	Handles actions after the challenge is written
	*/
	void handleWriteChallenge(const boost::system::error_code& error);

	/**
	Handles actions after a response is read
	*/
	void handleReadChallengeResponse(const boost::system::error_code& error);

	/**
	Handles actions after an auth failure status is written
	*/
	void handleWriteAuthFailure(const boost::system::error_code& error);

	/**
	Handles actions after an auth success message is written
	*/
	void handleWriteAuthSuccess(const boost::system::error_code& error);

};
