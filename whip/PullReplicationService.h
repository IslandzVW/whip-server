#pragma once

#include "RemoteAssetService.h"
#include "ExistenceIndex.h"
#include "IAsyncStorageBackend.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <queue>
#include <string>

class PullReplicationService
{
public:
	typedef boost::shared_ptr<PullReplicationService> ptr;

private:
	enum ReplicateStep
	{
		RS_NONE,
		RS_IDLIST,
		RS_RETRIEVE,
		RS_WAIT,
		RS_NOMASTER
	};

	boost::asio::ip::tcp::endpoint _replicationMasterEndpoint;
	int _pullFrequency;
	iwvfs::ExistenceIndex::ptr _existenceIndex;
	IAsyncStorageBackend::ptr _storageBackend;
	bool _stop;
	boost::thread _serviceThread;
	boost::asio::io_service _ioService;

	RemoteAssetService::ptr _assetSvc;

	bool _connected;

	boost::asio::deadline_timer _reconnectTimer;

	boost::shared_ptr<boost::asio::io_service::work> _work;

	ReplicateStep _currentStep;
	int _currentAssetPrefix;
	std::queue<std::string> _pendingAssets;
	std::set<std::string> _inProgressAssets;

	boost::asio::deadline_timer _replRestartTimer;

	boost::posix_time::ptime _nextReplStart;

	unsigned int _batchSize;


	void run();
	void runStep();
	bool verifyConnection();

	void onServiceConnectionStatus(RemoteAssetService::ptr ras, bool success);

	void replicateStep();

	void retrieveIdListForPrefix();

	std::string getStringPrefixForIndex();

	void onGetStoredAssetIds(Asset::ptr asset, RASError error, std::string ids);

	void retrieveNextIdList();

	void retrieveNextMissingAssetBatch();

	void onAssetRetrieved(Asset::ptr asset, RASError error, std::string errorMesg, std::string assetId);

	void onAfterAssetStoredStub(bool success, AssetStorageError::ptr error, std::string assetId);

	void onAfterAssetStored(bool success, AssetStorageError::ptr error, std::string assetId);

	void onServerConnectionFailed(RemoteAssetService::ptr remoteServer);

	void setReplicationRestartTimer();

	void checkResumeNextReplRound();

	void doStop();

public:
	PullReplicationService(const boost::asio::ip::tcp::endpoint& replicationMasterEndpoint,
		int pullFrequency, iwvfs::ExistenceIndex::ptr existenceIndex,
		IAsyncStorageBackend::ptr storageBackend);
	virtual ~PullReplicationService();

	void start();
	void stop();
};

