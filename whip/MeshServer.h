#pragma once

#include "AuthChallengeMsg.h"
#include "AuthResponseMsg.h"
#include "AuthStatusMsg.h"
#include "IntraMeshMsg.h"
#include "Asset.h"
#include "ClientRequestMsg.h"
#include "ServerResponseMsg.h"
#include "MeshQueryConnection.h"

#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/tuple/tuple.hpp>

#include <set>
#include <map>
#include <queue>

namespace iwintramesh
{
	class AssetSearch;
	typedef boost::weak_ptr<AssetSearch> AssetSearchWPtr;
	typedef boost::shared_ptr<AssetSearch> AssetSearchPtr;

	/**
	 * Represents a peer server on the mesh network that this server knows about
	 * due to getting a heartbeat signal
	 */
	class MeshServer : public boost::enable_shared_from_this<MeshServer>
	{
	public:
		typedef boost::shared_ptr<MeshServer> ptr;
		typedef boost::weak_ptr<MeshServer> wptr;

		typedef boost::function<void (MeshServer::ptr, bool)> ConnectCallback;
		typedef boost::function<void (MeshServer::ptr)> SafeKillCallback;

	private:
		const static short TOTAL_CONNECTION_COUNT = 2;

		/**
		 * Time that elapses (in seconds) before this server is considered timed out and
		 * connections closed
		 */
		const static short HEARTBEAT_TIMEOUT = 30;

		/**
		 * The last time we received a heartbeat from this server
		 */
		boost::posix_time::ptime _lastHeartbeat;

		/**
		 * IP address and port of the endpoint of this server's intramesh
		 * listening port
		 */
		boost::asio::ip::tcp::endpoint _meshQueryEndpoint;

		/**
		 * The port that the tcp/ip asset service is running on
		 */
		boost::asio::ip::tcp::endpoint _assetServiceEndpoint;

		/**
		 * IO service that will service all requests for this server
		 */
		boost::asio::io_service& _ioService;

		/**
		 * Tcp/Ip socket for accessing asset services on the remote
		 */
		boost::asio::ip::tcp::socket _serviceSocket;

		/**
		 * Last heartbeat flags seen
		 */
		int _lastHeartbeatFlags;

		/**
		 * Whether or not a successful connection was made to the asset service port
		 */
		bool _assetConnectionEstablished;

		/**
		 * Whether or not a successful connection was made to the mesh query port
		 */
		bool _meshQueryConnectionEstablished;
		
		/**
		 * Current inbound auth challenge message
		 */
		AuthChallengeMsg::ptr _inboundAuthChallenge;

		typedef std::set<AssetSearchWPtr> AssetSearchSet;
		typedef std::map<std::string, AssetSearchSet> AssetIdToSearchMap;
		/**
		 * Tracks the current searches in progress
		 */
		AssetIdToSearchMap _currentSearches;

		/**
		 * Callback to make after connection status is known
		 */
		ConnectCallback _connectCallback;

		/**
		 * Callback to make when all searches are completed
		 */
		SafeKillCallback _safeKillCallback;

		/**
		 * Stores all pending transfer requests
		 */
		typedef boost::tuples::tuple<std::string, boost::function<void (Asset::ptr)> > PendingTransferRequest;
		typedef std::queue<PendingTransferRequest> TransferRequestQueue;
		TransferRequestQueue _transferRequestQueue;
		TransferRequestQueue _assetCallbackQueue;

		/**
		 * Connection to the query service on the remote server
		 */
		MeshQueryConnection::ptr _meshQueryConnection;

		/**
		 * The number of connections that returned a result. If this is equal to 2,
		 * all server connections have been attempted
		 */
		short _numConnectResultsRcvd;

		/**
		 * Indicates whether there is an asset request send taking place
		 */
		bool _sendActive;

		/**
		 * Indicates whether there is an asset receive in progress
		 */
		bool _recvActive;




		void onConnect(const boost::system::error_code& error);

		void onMeshServiceConnect(const boost::system::error_code& error);
		
		void onRecvChallenge(const boost::system::error_code& error, size_t bytesRcvd);
		
		void onChallengeResponseWrite(const boost::system::error_code& error, size_t bytesSent,
			AuthResponseMsg::ptr authResponse);

		void onAuthenticationStatus(const boost::system::error_code& error, size_t bytesRcvd,
			AuthStatusMsg::ptr authStatus);

		/**
		 * Responds to the current auth challenge on the wire
		 */
		void sendChallengeResponse();
		
		void onQueryAnswerRcvd(IntraMeshMsg::ptr inbound);

		/**
		 * Sends a not found response to all searches listening for
		 * the given UUID
		 */
		void postNegativeResponse(const std::string& uuid);

		/**
		 * Sends a "found" response to all searches listening for
		 * the given UUID
		 */
		void postPositiveResponse(const std::string& uuid);

		void postResponse(const std::string& uuid, bool found);


		void onAfterWriteAssetRequest(const boost::system::error_code& error, size_t bytesSent,
			ClientRequestMsg::ptr outbound);

		void onReadResponseHeader(const boost::system::error_code& error, size_t bytesSent,
			ServerResponseMsg::ptr response);

		void onReadResponseData(const boost::system::error_code& error, size_t bytesSent,
			ServerResponseMsg::ptr response);

		void onHandleRequestErrorData(const boost::system::error_code& error, size_t bytesSent,
			ServerResponseMsg::ptr response);


		/**
		 * Checks if a safe kill callback is registered and if we are ready to
		 * be killed. If we are, calls the safe kill Callback.
		 *
		 * WARNING: Proceeding this call, if it is safe to destroy this object *this* will be destroyed
		 */
		void checkSafeKill();

		/**
		 * Called when the query endpoint is closed or closes
		 */
		void queryEndpointClosed();

		/**
		 * Called by the query connection when it's connection is successful or fails
		 */
		void onQueryConnectionConnectResult(const boost::system::error_code& error);

		/**
		 * Called when connections are established or fail to the remote server
		 */
		void onConnectionResult();

		/**
		 * Listens for asset data coming over the wire
		 */
		void listenForAssetData();


		void sendGetRequest(const std::string& uuid, boost::function<void (Asset::ptr)> callBack);

	public:
		MeshServer(const boost::asio::ip::tcp::endpoint& meshQueryEndpoint,
			const boost::asio::ip::tcp::endpoint& assetServiceEndpoint, 
			boost::asio::io_service& ioService, int initialFlags);
		virtual ~MeshServer();

		/**
		 * Closes all service sockets
		 */
		void close();

		/**
		 * Returns true if the time since the last heartbeat exceeds the timeout,
		 * otherwise returns false
		 */
		bool isTimedOut() const;

		/**
		 * Returns the endpoint this server is listening on
		 */
		const boost::asio::ip::tcp::endpoint& getQueryEndpoint() const;

		/**
		 * Updates the last heartbeat for this server to now
		 */
		void updateHeartbeat(int flags);

		/**
		 * Current status for this server
		 */
		int getHeartbeatFlags() const;

		/**
		 * Connects to the mesh server's asset service
		 */
		void connect(ConnectCallback connectCallback);

		/**
		 * Returns whether or not the asset service mesh connection is established
		 */
		bool serviceConnectionEstablished() const;

		/**
		 * Queries for the given asset and calls the given function 
		 * when the result is ready
		 */
		void search(AssetSearchWPtr assetSearch);

		/**
		 * Cancels the current pending query operation on this server
		 */
		void cancelSearch(AssetSearchPtr assetSearch);

		/**
		 * Retrieves the given asset from this server
		 */
		void getAsset(const std::string& uuid, boost::function<void (Asset::ptr)> callBack);

		/**
		 * Registers a class for notification when this server can be safely deleted (no pending searches)
		 */
		void registerSafeKillCallback(SafeKillCallback safeKillCallback);

		/**
		 * Returns a human readable description of this mesh server
		 */
		std::string getDescription();
	};

	struct _MeshServerCmp
	{
		bool operator()(const MeshServer::ptr l, const MeshServer::ptr r) const
		{
			return l->getQueryEndpoint() < r->getQueryEndpoint();
		}
	};
}