#include "StdAfx.h"

#include "AssetServer.h"
#include "AssetCache.h"
#include "Settings.h"
#include "VFSBackend.h"
#include "IntraMeshService.h"

#include <boost/bind.hpp>
#include <ostream>
#include <boost/foreach.hpp>

using boost::asio::ip::tcp;
using namespace iwintramesh;
using namespace boost::posix_time;

extern boost::asio::io_service* AppIoService;

const unsigned short AssetServer::DEFAULT_PORT = 32700;
const unsigned short AssetServer::STAT_TIMER_INTERVAL = 5;

AssetServer::AssetServer(boost::asio::io_service& ioService, unsigned short port)
:	_ioService(ioService), _port(port), 
	_endpoint(tcp::v4(), _port),
	_acceptor(ioService, _endpoint),
	_statTimer(_ioService),
	_lastCollection(second_clock::local_time()),
	_shutDown(false)
{
	this->configureIntramesh();
	this->configurePushReplication();
	this->configureStorage();
	this->configurePullReplication();

	SAFELOG(AppLog::instance().out() << "Starting asset services on TCP/" << _port << std::endl);
}

AssetServer::~AssetServer()
{
}

void AssetServer::configureIntramesh()
{
	unsigned short port = whip::Settings::instance().get("intramesh_port").as<unsigned short>();
	_intraMeshService.reset(new IntraMeshService(_ioService, port));
}

void AssetServer::start()
{
	this->acceptNew();
	_intraMeshService->start();
	this->restartStatTimer();
}

void AssetServer::restartStatTimer()
{
	_statTimer.expires_from_now(seconds(STAT_TIMER_INTERVAL));
	_statTimer.async_wait(boost::bind(&AssetServer::onCollectStats, this,
        boost::asio::placeholders::error));
}

void AssetServer::onCollectStats(const boost::system::error_code& error)
{
	if (! error) {
		ptime currTime = second_clock::local_time();
		long elapsed = (currTime - _lastCollection).total_seconds();
		if (elapsed == 0) return;

		int requests = IntraMeshService::GetMeshRequestsStat();

		SAFELOG(AppLog::instance().out() 
			<< std::endl 
			<< "[STATS](Client) Requests/sec: " << (AssetClient::GetNumRequestsStat()/elapsed) << ", "
			<< "Data Rate: " << (AssetClient::GetKBTransferredStat()/elapsed) << "kB/sec" << std::endl
			<< "[STATS](Mesh) #Requests: " << requests << "(" << (requests/elapsed) << "/sec), "
			<< "Positive Req: " << IntraMeshService::GetPositiveMeshResponseStat() << ", "
			<< "Data Rate: " << (AssetClient::GetMeshKBTransferredStat()/elapsed) << "kB/sec" << std::endl);

		this->restartStatTimer();
		_lastCollection = second_clock::local_time();
	}
}

IAsyncStorageBackend::ptr AssetServer::selectStorageBackend(const std::string& backendName, const std::string& migration, 
													   const std::string& storageRoot, bool enablePurge)
{
	IAsyncStorageBackend::ptr backend;

	if (boost::to_lower_copy(backendName) == "vfs") {
		boost::shared_ptr<iwvfs::VFSBackend> vfs(new iwvfs::VFSBackend(storageRoot, enablePurge, _ioService, _pushReplicationSvc));

		_index = vfs->getIndex();

		backend = vfs;
		return backend;
	} 
	
	throw std::runtime_error("disk_storage_backend must be vfs");
}

void AssetServer::configureStorage()
{
	bool cachingEnabled = whip::Settings::instance().get("cache_enabled").as<bool>();
	std::string diskStorageRoot = whip::Settings::instance().get("disk_storage_root").as<std::string>();
	if (diskStorageRoot.empty()) throw std::runtime_error("disk_storage_root must be specified");

	bool purgeEnabled = whip::Settings::instance().get("allow_purge").as<bool>();
	std::string diskStorageBackend = whip::Settings::instance().get("disk_storage_backend").as<std::string>();
	std::string migration = whip::Settings::instance().get("migration").as<std::string>();

	IAsyncStorageBackend::ptr diskBackend(this->selectStorageBackend(diskStorageBackend, migration, diskStorageRoot, purgeEnabled));
	_diskStorage = diskBackend;

	if (cachingEnabled) {
		unsigned long long cacheSize = static_cast<unsigned long long>(whip::Settings::instance().get("cache_size").as<unsigned int>()) * 1000000LL;
		AssetCache::ptr assetCache(new AssetCache(cacheSize, diskBackend, _ioService));
		_storage = assetCache;
		_intraMeshService->setCache(assetCache);

	} else {
		_storage = _diskStorage;
	}

	_intraMeshService->setLocalBackend(_storage);
	_intraMeshService->setLocalDiskBackend(_diskStorage);
}

void AssetServer::acceptNew()
{
	AssetClient::ptr acceptClient(new AssetClient(_ioService, *this, _storage, _intraMeshService->getMeshBackend()));
	acceptClient->setInitialState();

	_acceptor.async_accept(acceptClient->getConn(),
        boost::bind(&AssetServer::handleAccept, this, acceptClient,
          boost::asio::placeholders::error));
}

void AssetServer::handleAccept(AssetClient::ptr client, const boost::system::error_code& error)
{
	if (! error) {
		client->start();
		this->acceptNew();

	} else {
		SAFELOG(AppLog::instance().error() << "Error during accept on client connect: " << error.message() << std::endl);
		
		if (! _shutDown) {
			this->acceptNew();
		}
	}
}

void AssetServer::clientConnect(AssetClient::ptr client)
{
	_clients.insert(client);
}

void AssetServer::clientDisconnect(AssetClient::ptr client)
{
	_clients.erase(client);
}

void AssetServer::getStatus(boost::shared_ptr<std::ostream> journal)
{
	(*journal) << "-General" << std::endl;
	(*journal) << "  Clients Connected: " << _clients.size() << std::endl;
	(*journal) << "-Client Status" << std::endl;

	BOOST_FOREACH(AssetClient::ptr client, _clients)
	{
		(*journal) << "  " << client->getStatus() << std::endl;
	}
}

void AssetServer::configurePullReplication()
{
	std::string master = whip::Settings::instance().replicationMaster();
	if (master == "") {
		SAFELOG(AppLog::instance().out() << "[REPL] No master server set. Replication disabled." << std::endl);
	} else {
		boost::asio::ip::tcp::endpoint ep(whip::Settings::ParseHostPortPairToTcpEndpoint(master));

		_pullReplicationSvc.reset(new PullReplicationService(ep, whip::Settings::instance().get("pull_replication_frequency").as<int>(), _index, _diskStorage));
		_pullReplicationSvc->start();
	}
}

void AssetServer::configurePushReplication()
{
	_pushReplicationSvc.reset(new PushReplicationService());
	_pushReplicationSvc->start();
}

void AssetServer::stop()
{
	_shutDown = true;
	
	_pushReplicationSvc->stop();

	if (_pullReplicationSvc) {
		_pullReplicationSvc->stop();
	}

	_storage->shutdown();
	_intraMeshService->stop();

	AppIoService->stop();
}
