#pragma once

#include "IConnectionState.h"
#include "AuthChallengeMsg.h"
#include "IAsyncStorageBackend.h"
#include "MeshStorageBackend.h"

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <string>

class AssetServer;

/**
Represents a client connection to the asset server
*/
class AssetClient : public boost::enable_shared_from_this<AssetClient>
{
private:
	boost::asio::io_service& _ioService;
	AssetServer& _assetServer;
	boost::asio::ip::tcp::socket _conn;
	boost::asio::ip::tcp::endpoint _ep;
	IConnectionState::ptr _state;
	IAsyncStorageBackend::ptr _storage;
	iwintramesh::MeshStorageBackend::ptr _meshStorage;

	//statistics 
	static int _reqSinceLastPoll;
	static int _kbTransferredSinceLastPoll;
	static int _meshKbTransferredSinceLastPoll;

public:
	/**
	Pointer type
	*/
	typedef boost::shared_ptr<AssetClient> ptr;

public:
	//statistics

	static int GetNumRequestsStat();
	static int GetKBTransferredStat();
	static int GetMeshKBTransferredStat();

	static void AddRequestToStats();
	static void AddTransferToStats(int bytes);
	static void AddMeshTransferToStats(int bytes);

public:
	AssetClient(boost::asio::io_service& ioService, AssetServer& server, IAsyncStorageBackend::ptr storage,
		iwintramesh::MeshStorageBackend::ptr meshStorage);
	virtual ~AssetClient();

	/**
	Tells this client to set it's initial state.  Must be done outside of the 
	ctor because of shared_from_this use
	*/
	void setInitialState();

	/**
	Returns the socket that is associated with this client
	*/
	boost::asio::ip::tcp::socket& getConn();
	
	/**
	Changes the connection state for this client
	*/
	void setState(IConnectionState::ptr state);

	/**
	Starts the current state
	*/
	void startState();

	/**
	Starts the client work loop
	*/
	void start();

	/**
	Disconnects the client and detaches from the server
	*/
	void disconnect();

	/**
	 * Returns the server object this client belongs to
	 */
	AssetServer& getParentServer();

	std::string getStatus();
};
