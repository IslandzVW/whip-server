#pragma once

#include "IAsyncStorageBackend.h"
#include "IntraMeshMsg.h"
#include "MeshServer.h"
#include "AssetCache.h"
#include "IntraMeshQueryClient.h"

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/function.hpp>

#include <ostream>
#include <map>
#include <set>

namespace iwintramesh
{
	class MeshStorageBackend;
	typedef boost::shared_ptr<MeshStorageBackend> MeshStorageBackendPtr;

	/**
	 * Manages communication between servers in the mesh network.  This includes
	 * listening to heartbeats, managing connections to external servers and
	 * providing the mesh storage backend with up to date information regarding remote
	 * servers
	 */
	class IntraMeshService : public boost::noncopyable
	{
	public:
		/**
		 * Default port number for intramesh protocol if none specified
		 */
		static const unsigned short DEFAULT_PORT;
		
		/**
		 * Number of seconds between heartbeat signals coming from this server
		 */
		static const unsigned short HEARTBEAT_INTERVAL;


		typedef boost::shared_ptr<IntraMeshService> ptr;
		typedef boost::weak_ptr<IntraMeshService> wptr;


		static int GetMeshRequestsStat();
		static int GetPositiveMeshResponseStat();

	private:
		static int _meshReqSinceLastPoll;
		static int _positiveRespSinceLastPoll;

		boost::asio::io_service& _masterIoService;
		boost::asio::ip::tcp::endpoint _endpoint;
		boost::asio::ip::tcp::acceptor _acceptor;

		IAsyncStorageBackend::ptr _localBackend;
		IAsyncStorageBackend::ptr _diskBackend;

		bool _debugging;

		/**
		 * Known good peer server connections
		 */
		typedef std::map<boost::asio::ip::tcp::endpoint, MeshServer::ptr> ServerMap;
		ServerMap _remoteServers;

		boost::asio::deadline_timer _heartBeatTimer;

		/**
		 * The mesh service backend we're managing
		 */
		MeshStorageBackendPtr _meshStorageBackend;

		/**
		 * Stores pending server connections
		 */
		ServerMap _pendingServerConnections;

		/**
		 * Stores server connections that failed to connect until they're ready to be deleted
		 */
		std::set<MeshServer::ptr> _connectFailedServers;

		/**
		 * Stores clients connected to this intramesh server
		 */
		typedef std::set<IntraMeshQueryClient::ptr> ClientSet;
		ClientSet _connectedClients;

		/**
		 * Whether this service is shutting down
		 */
		bool _stop;



		/**
		 * Connects to the mesh server that just sent us a heartbeat
		 */
		void connectToMeshServer(int initialFlags, const whip::IntraMeshPeerEntry& peer);

		/**
		 * Determines if this server is on the trusted list or not
		 */
		bool isServerTrusted(const std::string& address);

		/**
		 * Handles the server heartbeat
		 */
		void onHeartBeat(const boost::system::error_code& error);

		/**
		 * Resets the heartbeat timer after use
		 */
		void resetHeartBeatTimer();

		/**
		 * Runs the heartbeat process
		 */
		void doHeartBeat();

		/**
		 * Generates the flags for the heartbeat on this server
		 */
		int generateHeartBeatFlags();

		/**
		 * Called by a mesh server when there are no more pending I/O ops
		 */
		void onSafeToKill(MeshServer::ptr meshServer);

		/**
		 * Returns if a connection is already pending to the given server
		 */
		bool connectionIsPending(const boost::asio::ip::tcp::endpoint& ep);

		/**
		 * Accepts a new client to this intramesh service
		 */
		void acceptNextClient();

		void handleAfterAccept(const boost::system::error_code& error, IntraMeshQueryClient::ptr client);

		void onConnectFailedSafekill(MeshServer::ptr meshServer);


	public:
		IntraMeshService(boost::asio::io_service& masterIoService, unsigned short port);
		virtual ~IntraMeshService();

		/**
		 * Sets the local disk backend used to service intramesh MESH_QUERY questions
		 * this is so that a server doesnt request an asset that is only in the cache
		 * and prevents a negative response in the event that the item was only in the 
		 * cache of this server and was purged between the MESH_QUERY and the actual
		 * asset retrieval
		 */
		void setLocalDiskBackend(IAsyncStorageBackend::ptr diskBackend);

		/**
		 * Sets the local storage backend for this service to use
		 */
		void setLocalBackend(IAsyncStorageBackend::ptr localBackend);

		/**
		 * Begins listening for asset queries on the local mesh
		 */
		void start();

		/**
		 * Runs the given visitor for each connected server
		 */
		void forEachServer(boost::function<void (MeshServer::ptr)> visitor);

		/**
		 * Returns the mesh backend managed by this service
		 */
		MeshStorageBackendPtr getMeshBackend();

		/**
		 * Called when a connection attempt has been completed to a mesh server
		 */
		void onConnectAttemptCompleted(MeshServer::ptr meshServer, bool connectSuccess);

		/**
		 * Informs the mesh storage backend of cache availability
		 */
		void setCache(AssetCache::ptr assetCache);

		/**
		 * Returns information about the service and topology
		 */
		void getStatus(boost::shared_ptr<std::ostream> journal);

		/**
		 * Adds 1 to the mesh request counter
		 */
		void addMeshRequestStat();

		/**
		 * Adds 1 to the positive mesh request counter
		 */
		void addPositiveMeshRequestStat();

		/**
		 * Tells this service one of its clients has disconnected
		 */
		void queryClientDisconnected(IntraMeshQueryClient::ptr client);

		/**
		 * Tells this service to prepare for a shutdown
		 */
		void stop();
	};

}