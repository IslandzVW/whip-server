#include "StdAfx.h"
#include "PushReplicationService.h"

#include "AppLog.h"

#include <string>
#include <algorithm>


PushReplicationService::PushReplicationService()
	: _ioService(), _connected(false), _connectionMonitor(_ioService), _sending(false)
{
}


PushReplicationService::~PushReplicationService()
{
}

void PushReplicationService::start()
{
	_stop = false;
	_serviceThread = boost::thread(boost::bind(&PushReplicationService::run, this));
}

void PushReplicationService::run()
{
	_work.reset(new boost::asio::io_service::work(_ioService));
	this->resetConnectionMonitor(1);
	_ioService.run();
}

void PushReplicationService::resetConnectionMonitor(int interval)
{
	if (_stop) return;

	_connectionMonitor.expires_from_now(boost::posix_time::seconds(interval));
	_connectionMonitor.async_wait(boost::bind(&PushReplicationService::onCheckConnection, this));
}

void PushReplicationService::onCheckConnection()
{
	if (_stop) return;

	//look up the slave endpoint
	std::string slaveHpp(whip::Settings::instance().replicationSlave());

	if (slaveHpp != "") {
		boost::asio::ip::tcp::endpoint ep(whip::Settings::ParseHostPortPairToTcpEndpoint(slaveHpp));
		if (ep != _replicationSlaveEndpoint) {
			this->endpointChanged(ep);
		} else {
			if (! _connected) {
				//slave didnt change, but we need to connect
				this->makeConnection();
			} else {
				this->resetConnectionMonitor();
			}
		}

	} else {
		if (_connected) _replSlave->close();
		this->resetConnectionMonitor();
	}
}

void PushReplicationService::endpointChanged(boost::asio::ip::tcp::endpoint newEp)
{
	_replicationSlaveEndpoint = newEp;

	if (_connected) {
		_replSlave->close();
		this->resetConnectionMonitor();
	} else {
		this->makeConnection();
	}
}

void PushReplicationService::makeConnection()
{
	SAFELOG(AppLog::instance().out() << "[REPL] Connecting to push replication slave " << _replicationSlaveEndpoint << std::endl);
	_replSlave.reset(new RemoteAssetService(_replicationSlaveEndpoint, _ioService));
	_replSlave->setSafekillCallback(boost::bind(&PushReplicationService::onSafeKillConnection, this, _1));
	_replSlave->connect(boost::bind(&PushReplicationService::onServiceConnectionStatus, this, _1, _2));
}

void PushReplicationService::onSafeKillConnection(RemoteAssetService::ptr assetSvc)
{
	_connected = false;
	_sending = false;
	
	std::queue<Asset::ptr> empty;
	std::swap(_waitingAssets, empty);
}

void PushReplicationService::onServiceConnectionStatus(RemoteAssetService::ptr ras, bool success)
{
	if (success) {
		SAFELOG(AppLog::instance().out() << "[REPL] Connected to push replication slave " << _replicationSlaveEndpoint << std::endl);
		_connected = true;
	} else {
		SAFELOG(AppLog::instance().error() << "[REPL] Connection to push replication slave " << _replicationSlaveEndpoint << " failed" << std::endl);
	}

	this->resetConnectionMonitor();
}

void PushReplicationService::queueAssetForPush(Asset::ptr asset)
{
	_ioService.post(boost::bind(&PushReplicationService::doQueueAssetForPush, this, asset));
}

void PushReplicationService::doQueueAssetForPush(Asset::ptr asset)
{
	if (! _connected || _stop) return;

	if (_sending) {
		if (_waitingAssets.size() > MAX_QUEUE_SIZE) {
			return;
		}

		_waitingAssets.push(asset);

	} else {
		this->doPutAsset(asset);
	}
}

void PushReplicationService::doPutAsset(Asset::ptr asset)
{
	if (_stop) return;

	_sending = true;
	_replSlave->putAsset(asset, boost::bind(&PushReplicationService::onAfterAssetPut, this, _1, _2, _3));
}

void PushReplicationService::onAfterAssetPut(Asset::ptr asset, RASError error, std::string data)
{
	_sending = false;

	if (error) {
		SAFELOG(AppLog::instance().error() << "[REPL] Push replication failed asset: " << data << std::endl);
	}

	this->pushNextWaitingAsset();
}

void PushReplicationService::pushNextWaitingAsset()
{
	if (_connected && !_stop) {
		if (_waitingAssets.size() > 0) {
			Asset::ptr asset(_waitingAssets.front());
			_waitingAssets.pop();

			this->doPutAsset(asset);
		}
	}
}

void PushReplicationService::stop()
{
	_ioService.post(boost::bind(&PushReplicationService::doStop, this));
	_serviceThread.join();
}

void PushReplicationService::doStop()
{
	_stop = true;
	_work.reset();
	if (_connected) _replSlave->close();
}
