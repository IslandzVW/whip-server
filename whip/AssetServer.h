#pragma once

#include "AssetClient.h"
#include "IStorageBackend.h"
#include "IntraMeshService.h"
#include "PullReplicationService.h"
#include "ExistenceIndex.h"
#include "PushReplicationService.h"

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <set>
#include <iosfwd>

/**
	An instance of the asset server 
*/
class AssetServer : public boost::enable_shared_from_this<AssetServer>
{
public:
	/**
	Default port number for asset server if none specified
	*/
	static const unsigned short DEFAULT_PORT;

private:
	/**
	 * Stat collection interval
	 */
	static const unsigned short STAT_TIMER_INTERVAL;

	boost::asio::io_service& _ioService;
	unsigned short _port;
	boost::asio::ip::tcp::endpoint _endpoint;
	boost::asio::ip::tcp::acceptor _acceptor;
	IAsyncStorageBackend::ptr _storage;
	IAsyncStorageBackend::ptr _diskStorage;
	iwintramesh::IntraMeshService::ptr _intraMeshService;
	iwvfs::ExistenceIndex::ptr _index;
	
	/**
	A list of all clients connected
	*/
	std::set<AssetClient::ptr> _clients;

	/**
	 * Timer fired once every 15 seconds to display stats
	 */
	boost::asio::deadline_timer _statTimer;

	/**
	 * The last time stats were collected
	 */
	boost::posix_time::ptime _lastCollection;

	/**
	 * Are we shutting down?
	 */
	bool _shutDown;

	/**
	 * Our pull replication service, if we are running replication. Otherwise null
	 */
	PullReplicationService::ptr _pullReplicationSvc;

	/**
	 * Our push replication service
	 */
	PushReplicationService::ptr _pushReplicationSvc;



	IAsyncStorageBackend::ptr selectStorageBackend(const std::string& backendName, const std::string& migration, 
		const std::string& storageRoot, bool enablePurge);

	void acceptNew();
	void configureStorage();
	void configureIntramesh();
	void restartStatTimer();
	void onCollectStats(const boost::system::error_code& error);
	void configurePushReplication();
	void configurePullReplication();

public:
	/**
	Start a new instance of the asset server on the given port
	*/
	AssetServer(boost::asio::io_service& ioService, unsigned short port);
	virtual ~AssetServer();

	/**
	Begins handling connections
	*/
	void start();

	/**
	Handler for async accept on client connection
	*/
	void handleAccept(AssetClient::ptr client, const boost::system::error_code& error);
 
	/**
	Adds the given client to the list of active clients
	*/
	void clientConnect(AssetClient::ptr client);

	/**
	Removes the given client from the list of active clients
	*/
	void clientDisconnect(AssetClient::ptr client);

	/**
	 * Writes status information to the given ostream
	 */
	void getStatus(boost::shared_ptr<std::ostream> journal);

	/**
	 * Stops servicing clients
	 */
	void stop();
};	
