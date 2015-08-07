#include "StdAfx.h"
#include "PullReplicationService.h"

#include "AppLog.h"
#include "Settings.h"

//compiler complains about std::copy inside of boost
#pragma warning(disable:4996)
#include <boost/algorithm/string.hpp>
#pragma warning(default:4996)

#include <boost/foreach.hpp>


PullReplicationService::PullReplicationService(const boost::asio::ip::tcp::endpoint& replicationMasterEndpoint,
	int pullFrequency, iwvfs::ExistenceIndex::ptr existenceIndex, IAsyncStorageBackend::ptr storageBackend)
	: _replicationMasterEndpoint(replicationMasterEndpoint), _pullFrequency(pullFrequency), _existenceIndex(existenceIndex),
		_storageBackend(storageBackend), _stop(false), _serviceThread(), _ioService(), _connected(false),
		_reconnectTimer(_ioService), _currentStep(RS_NONE), _replRestartTimer(_ioService)
{
	_batchSize = whip::Settings::instance().get("pull_replication_batch_size").as<unsigned int>();
}


PullReplicationService::~PullReplicationService()
{
}

void PullReplicationService::start()
{
	SAFELOG(AppLog::instance().out() << "[REPL] Starting pull replication service for master " << _replicationMasterEndpoint << std::endl);

	_work.reset(new boost::asio::io_service::work(_ioService));
	_stop = false;
	_serviceThread = boost::thread(boost::bind(&PullReplicationService::run, this));
}

void PullReplicationService::run()
{
	this->runStep();
	_ioService.run();
	SAFELOG(AppLog::instance().out() << "[REPL] Pull replication thread finished " << _replicationMasterEndpoint << std::endl);
}

void PullReplicationService::runStep()
{
	if (_stop) return;

	//first thing's first. we need a connection
	if (! this->verifyConnection()) return;
	//if we have a connection we can resume or start replication
	this->replicateStep();
}

bool PullReplicationService::verifyConnection()
{
	if (_stop) return false;

	if (_currentStep == RS_NONE || _currentStep == RS_WAIT || _currentStep == RS_NOMASTER) {
		std::string master = whip::Settings::instance().replicationMaster();

		if (master != "") {
			boost::asio::ip::tcp::endpoint ep(whip::Settings::ParseHostPortPairToTcpEndpoint(master));

			if (ep != _replicationMasterEndpoint) {
				SAFELOG(AppLog::instance().out() << "[REPL] Replication master has changed, reconnecting to new master " << ep << std::endl);
				_currentStep = RS_NONE;
				_replicationMasterEndpoint = ep;

				if (_assetSvc && _connected) {
					_assetSvc->close();
					return false;
				}
			}

		} else {
			if (_currentStep != RS_NOMASTER) {
				SAFELOG(AppLog::instance().out() << "[REPL] This server is no longer a replication pull slave" << std::endl);
				_currentStep = RS_NOMASTER;
				_assetSvc->close();
			}

			return true;
		}
	}

	if (! _assetSvc) {
		//create a new asset service
		_assetSvc.reset(new RemoteAssetService(_replicationMasterEndpoint, _ioService));
		_assetSvc->setSafekillCallback(boost::bind(&PullReplicationService::onServerConnectionFailed, this, _1));
	}

	if (! _connected) {
		SAFELOG(AppLog::instance().out() << "[REPL] Attempting connection to replication master " << _replicationMasterEndpoint << std::endl);
		_assetSvc->connect(boost::bind(&PullReplicationService::onServiceConnectionStatus, this, _1, _2));
		return false;
	}

	return true;
}

void PullReplicationService::onServiceConnectionStatus(RemoteAssetService::ptr service, bool success)
{
	if (! success) {
		//we failed. retry connection in 10 seconds
		_reconnectTimer.expires_from_now(boost::posix_time::seconds(10));
		_reconnectTimer.async_wait(boost::bind(&PullReplicationService::verifyConnection, this));
		return;
	}

	//Otherwise we're connected to the master server. So we should start/continue the replication process
	_connected = true;
	this->runStep();
}

void PullReplicationService::replicateStep()
{
	switch (_currentStep) {
	case RS_NONE:
		_currentAssetPrefix = whip::Settings::instance().get("pull_replication_start_at").as<int>();
		//fallthrough

	case RS_IDLIST:
		this->retrieveIdListForPrefix();
		break;

	case RS_RETRIEVE:
		this->retrieveNextMissingAssetBatch();
		break;

	case RS_WAIT:
		this->checkResumeNextReplRound();
		break;

	case RS_NOMASTER:
		this->setReplicationRestartTimer();
		break;
	}
}

void PullReplicationService::checkResumeNextReplRound()
{
	if (boost::posix_time::second_clock::universal_time() >= _nextReplStart) {
		_currentStep = RS_NONE;
		this->runStep();
	} else {
		this->setReplicationRestartTimer();
	}
}

void PullReplicationService::retrieveIdListForPrefix()
{
	_assetSvc->getStoredAssetIDsWithPrefix(this->getStringPrefixForIndex(), boost::bind(&PullReplicationService::onGetStoredAssetIds, this, _1, _2, _3));
}

std::string PullReplicationService::getStringPrefixForIndex()
{
	std::stringstream hexNum;
	hexNum << std::setfill('0') << std::setw(whip::Settings::UUID_PATH_CHARS) << std::hex << _currentAssetPrefix;

	return hexNum.str();
}

void PullReplicationService::onGetStoredAssetIds(Asset::ptr asset, RASError error, std::string ids)
{
	if (! _connected) {
		return;
	}

	if (! error) {
		//parse the stored ids to retain a list
		if (ids == "") {
			//no ids here, move on
			this->retrieveNextIdList();
			return;
		}

		//_pendingAssets.swap(std::queue<std::string>());
		std::queue<std::string> empty;
		std::swap(_pendingAssets, empty);

		std::vector<std::string> idList;
		boost::split(idList, ids, boost::is_any_of(","));

		BOOST_FOREACH(const std::string& id, idList)
		{
			if (id != "" && !_existenceIndex->assetExists(id)) {
				_pendingAssets.push(id);
			}
		}

		if (_pendingAssets.empty()) {
			this->retrieveNextIdList();
		} else {
			SAFELOG( AppLog::instance().out() << "[REPL] Requesting " << _pendingAssets.size() 
				<< " assets for pull [" << this->getStringPrefixForIndex() << "]" << std::endl );
			_currentStep = RS_RETRIEVE;
			this->runStep();
		}
	} else {
		SAFELOG( AppLog::instance().error() << "[REPL] Error requesting asset list from master: " << ids << std::endl );
		if (! error.isFatal()) this->runStep();
	}
	
}

void PullReplicationService::retrieveNextIdList()
{
	_currentStep = RS_IDLIST;
	_currentAssetPrefix++;

	if (_currentAssetPrefix <= 0xfff) {
		this->runStep();
	} else {
		//replication is finished
		_currentAssetPrefix = 0;
		_currentStep = RS_WAIT;

		_nextReplStart = boost::posix_time::second_clock::universal_time() + boost::posix_time::minutes(_pullFrequency);
		SAFELOG( AppLog::instance().out() << "[REPL] Pull replication run completed from " << _replicationMasterEndpoint 
			<< ". Next replication run will begin at " << _nextReplStart << std::endl );
		this->setReplicationRestartTimer();
	}
}

void PullReplicationService::setReplicationRestartTimer()
{
	_replRestartTimer.expires_from_now(boost::posix_time::seconds(5));
	_replRestartTimer.async_wait(boost::bind(&PullReplicationService::runStep, this));
}

void PullReplicationService::retrieveNextMissingAssetBatch()
{
	if (_pendingAssets.empty()) {
		this->retrieveNextIdList();

	} else {
		unsigned int requestsSent = 0;

		while (! _pendingAssets.empty()) {
			const std::string& nextAsset = _pendingAssets.front();

			if (_existenceIndex->assetExists(nextAsset)) {
				_pendingAssets.pop();
				continue;
			} else {
				_inProgressAssets.insert(nextAsset);
				requestsSent++;
				_assetSvc->getAsset(nextAsset, false, boost::bind(&PullReplicationService::onAssetRetrieved, this, _1, _2, _3, nextAsset));
				_pendingAssets.pop();
				
				if (requestsSent == _batchSize)
				{
					break;
				}
			}
		}

		if (requestsSent == 0)
		{
			//there was nothing to retrieve, we need to step again
			this->retrieveNextIdList();
		}
	}
}

void PullReplicationService::onAssetRetrieved(Asset::ptr asset, RASError error, std::string errorMesg, std::string assetId)
{
	if (! _connected) {
		return;
	}

	if (error) {
		//an asset error isn't fatal. the only way we disconnect is a protocol or connection error,
		//so we continue replication until we get a safekill message
		SAFELOG(AppLog::instance().error() << "[REPL] Unable to retrieve asset " << assetId 
			<< " from replication server " << _replicationMasterEndpoint << ": " << errorMesg << std::endl);

		_inProgressAssets.erase(assetId);
		if (! error.isFatal() && _inProgressAssets.size() == 0) 
		{
			this->runStep();
		}
	} else {
		//we have the asset, write it locally
		_storageBackend->storeAsset(asset, boost::bind(&PullReplicationService::onAfterAssetStoredStub, this, _1, _2, assetId));
	}
}

void PullReplicationService::onAfterAssetStoredStub(bool success, AssetStorageError::ptr error, std::string assetId)
{
	_ioService.post(boost::bind(&PullReplicationService::onAfterAssetStored, this, success, error, assetId));
}

void PullReplicationService::onAfterAssetStored(bool success, AssetStorageError::ptr error, std::string assetId)
{
	if (! success) {
		SAFELOG(AppLog::instance().error() << "[REPL] Unable to save asset " << assetId 
			<< " from replication server " << _replicationMasterEndpoint << ": " << error->what() << std::endl);
	}

	_inProgressAssets.erase(assetId);

	if (_inProgressAssets.size() == 0)
	{
		this->runStep();
	}
}

void PullReplicationService::doStop()
{
	_stop = true;
	_assetSvc->close();
	_work.reset();
}

void PullReplicationService::stop()
{
	_ioService.post(boost::bind(&PullReplicationService::doStop, this));
	_serviceThread.join();
}

void PullReplicationService::onServerConnectionFailed(RemoteAssetService::ptr remoteServer)
{
	if (_connected) {
		_connected = false;
		_assetSvc.reset();

		SAFELOG(AppLog::instance().error() << "[REPL] Master server connection to " <<
			_replicationMasterEndpoint << " failed " << std::endl);

		//a return from asset storage will call runstep, so we need to make sure we only call it once
		if (_inProgressAssets.size() == 0 && _currentStep != RS_NOMASTER && _currentStep != RS_WAIT) {
			this->runStep();
		} else if (_currentStep == RS_NOMASTER) {
			this->setReplicationRestartTimer();
		}
	}
}