#pragma once

#include "Asset.h"
#include "RemoteAssetService.h"

#include <boost/thread.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

/**
 * Manages replication of assets to a slave server by a push method
 * each time a new asset is written to a master server, this service
 * can have the new asset queued for transmission to a slave
 */
class PushReplicationService
{
public:
	typedef boost::shared_ptr<PushReplicationService> ptr;

private:
	/**
	 * Safety net for maximum allowed queued push assets before we start rejecting new writes
	 * rejection is ok because pull replication should catch anything we miss on its next pass
	 */
	static const int MAX_QUEUE_SIZE = 250;

	
	bool _stop;
	boost::thread _serviceThread;
	boost::shared_ptr<boost::asio::io_service::work> _work;
	boost::asio::io_service _ioService;

	bool _connected;
	boost::asio::deadline_timer _connectionMonitor;
	boost::asio::ip::tcp::endpoint _replicationSlaveEndpoint;

	RemoteAssetService::ptr _replSlave;
	bool _sending;
	std::queue<Asset::ptr> _waitingAssets;


	void run();

	void resetConnectionMonitor(int interval = 5);

	void onCheckConnection();

	void endpointChanged(boost::asio::ip::tcp::endpoint newEp);

	void onServiceConnectionStatus(RemoteAssetService::ptr ras, bool success);

	void doQueueAssetForPush(Asset::ptr asset);

	void onAfterAssetPut(Asset::ptr asset, RASError error, std::string data);

	void pushNextWaitingAsset();

	void doPutAsset(Asset::ptr asset);

	void makeConnection();

	void onSafeKillConnection(RemoteAssetService::ptr assetSvc);

	void doStop();

public:
	PushReplicationService();
	virtual ~PushReplicationService();

	/**
	 * Starts the service thread
	 */
	void start();

	/**
	 * Places an asset into the push queue for delivery to a slave server
	 */
	void queueAssetForPush(Asset::ptr asset);

	/**
	 * Gracefully stops and waits for the service thread to finish
	 */
	void stop();
};

