#include "StdAfx.h"

#include "IntraMeshService.h"
#include "AppLog.h"
#include "Settings.h"
#include "MeshStorageBackend.h"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/foreach.hpp>
#include <vector>
#include <algorithm>

using namespace boost::asio::ip;

namespace iwintramesh
{
	int IntraMeshService::_meshReqSinceLastPoll;
	int IntraMeshService::_positiveRespSinceLastPoll;

	/**
	 * Stats
	 */
	int IntraMeshService::GetMeshRequestsStat()
	{
		int ret = _meshReqSinceLastPoll;
		_meshReqSinceLastPoll = 0;

		return ret;
	}

	int IntraMeshService::GetPositiveMeshResponseStat()
	{
		int ret = _positiveRespSinceLastPoll;
		_positiveRespSinceLastPoll = 0;

		return ret;
	}

	const unsigned short IntraMeshService::DEFAULT_PORT = 32701;
	const unsigned short IntraMeshService::HEARTBEAT_INTERVAL = 5;


	IntraMeshService::IntraMeshService(boost::asio::io_service& masterIoService, unsigned short port)
		: _masterIoService(masterIoService), 
		_endpoint(tcp::v4(), port),
		_acceptor(masterIoService, _endpoint),
		_heartBeatTimer(_masterIoService),
		_stop(false)
	{
		_meshStorageBackend.reset(new MeshStorageBackend(*this, _masterIoService));

		_debugging = whip::Settings::instance().get("debug").as<bool>();

		//any errors with the settings file will be thrown here as an exception
		//hopefully in most cases this will be an early throw that'll catch
		//config errors
		whip::Settings::instance().intraMeshPeers();

		SAFELOG(AppLog::instance().out() << "[IWINTRAMESH] Intramesh starting on TCP/" << port << std::endl);
	}

	IntraMeshService::~IntraMeshService()
	{
	}

	void IntraMeshService::setCache(AssetCache::ptr assetCache)
	{
		_meshStorageBackend->setCache(assetCache);
	}

	void IntraMeshService::setLocalBackend(IAsyncStorageBackend::ptr localBackend)
	{
		_localBackend = localBackend;
	}

	void IntraMeshService::setLocalDiskBackend(IAsyncStorageBackend::ptr localDiskBackend)
	{
		_diskBackend = localDiskBackend;
	}

	void IntraMeshService::resetHeartBeatTimer()
	{
		_heartBeatTimer.expires_from_now(boost::posix_time::seconds(HEARTBEAT_INTERVAL));
		_heartBeatTimer.async_wait(boost::bind(&IntraMeshService::onHeartBeat, this,
            boost::asio::placeholders::error));
	}

	int IntraMeshService::generateHeartBeatFlags()
	{
		int flags = IntraMeshMsg::HB_ONLINE | IntraMeshMsg::HB_READABLE;
		if (whip::Settings::instance().get("is_writable").as<bool>()) {
			flags |= IntraMeshMsg::HB_WRITABLE;
		}

		return flags;
	}

	void IntraMeshService::doHeartBeat()
	{
		//send out a heartbeat to all currently connected peers
		int hbFlags = this->generateHeartBeatFlags();
		BOOST_FOREACH(IntraMeshQueryClient::ptr client, _connectedClients)
		{
			client->sendHeartbeat(hbFlags);
		}

		std::vector<whip::IntraMeshPeerEntry> peers = whip::Settings::instance().intraMeshPeers();
		
		//for any peers we're not currently connected or connecting to, initiate a connection
		BOOST_FOREACH(const whip::IntraMeshPeerEntry& peer, peers)
		{
			boost::asio::ip::tcp::endpoint queryEndPoint(boost::asio::ip::address::from_string(peer.Host), peer.IntraMeshPort);
			if (_remoteServers.find(queryEndPoint) == _remoteServers.end()) {
				this->connectToMeshServer(IntraMeshMsg::HB_ONLINE | IntraMeshMsg::HB_READABLE, peer);
			}
		}
	}

	void IntraMeshService::onSafeToKill(MeshServer::ptr meshServer)
	{
		_remoteServers.erase(meshServer->getQueryEndpoint());
	}

	void IntraMeshService::onHeartBeat(const boost::system::error_code& error)
	{
		try {
			//reload settings to pick up any new servers added
			whip::Settings::instance().reload();

			//run the heartbeat process
			this->doHeartBeat();

		} catch (const std::exception& e) {
			SAFELOG(AppLog::instance().out() << "[IWINTRAMESH] Exception thrown during heartbeat: " 
				<< e.what()
				<< std::endl);
		}

		this->resetHeartBeatTimer();
	}

	void IntraMeshService::start()
	{
		//accept new connections to the service 
		this->acceptNextClient();

		//also start our heartbeat timer
		this->resetHeartBeatTimer();
	}

	void IntraMeshService::acceptNextClient()
	{
		IntraMeshQueryClient::ptr acceptClient(new IntraMeshQueryClient(_masterIoService, *this, _diskBackend, _debugging));

		_acceptor.async_accept(acceptClient->getConn(),
			boost::bind(&IntraMeshService::handleAfterAccept, this,
			  boost::asio::placeholders::error, acceptClient));
	}

	void IntraMeshService::handleAfterAccept(const boost::system::error_code& error, IntraMeshQueryClient::ptr client)
	{
		if (! error) {
			if (this->isServerTrusted(client->getConn().remote_endpoint().address().to_string())) {
				_connectedClients.insert(client);
				client->start();
			} else {
				client->close();
			}

			this->acceptNextClient();

		} else {
			if (! _stop) {
				this->acceptNextClient();
			}
		}
	}

	bool IntraMeshService::isServerTrusted(const std::string& address)
	{
		//make sure this is a server we trust
		std::vector<whip::IntraMeshPeerEntry> peers = whip::Settings::instance().intraMeshPeers();
		
		bool trustServer = false;

		BOOST_FOREACH(const whip::IntraMeshPeerEntry& peer, peers)
		{
			if (peer.Host == address) {
				trustServer = true;
				break;
			}
		}
		
		return trustServer;
	}

	void IntraMeshService::connectToMeshServer(int initialFlags, const whip::IntraMeshPeerEntry& peer)
	{
		boost::asio::ip::tcp::endpoint queryEndPoint(boost::asio::ip::address::from_string(peer.Host), peer.IntraMeshPort);
		if (! this->connectionIsPending(queryEndPoint)) {
			boost::asio::ip::tcp::endpoint assetEndPoint(boost::asio::ip::address::from_string(peer.Host), peer.AssetServicePort);

			MeshServer::ptr newMeshServer(new MeshServer(queryEndPoint, assetEndPoint, _masterIoService, initialFlags));
			_pendingServerConnections.insert(ServerMap::value_type(queryEndPoint, newMeshServer));
			newMeshServer->connect(boost::bind(&IntraMeshService::onConnectAttemptCompleted, this, _1, _2));
		}
	}

	void IntraMeshService::onConnectAttemptCompleted(MeshServer::ptr meshServer, bool connectSuccess)
	{
		if (connectSuccess) {
			_remoteServers[meshServer->getQueryEndpoint()] = meshServer;
			meshServer->registerSafeKillCallback(boost::bind(&IntraMeshService::onSafeToKill, this, _1));
		} else {
			_connectFailedServers.insert(meshServer);
			meshServer->registerSafeKillCallback(boost::bind(&IntraMeshService::onConnectFailedSafekill, this, _1));
			meshServer->close();
		}

		_pendingServerConnections.erase(meshServer->getQueryEndpoint());
	}

	void IntraMeshService::onConnectFailedSafekill(MeshServer::ptr meshServer)
	{
		_connectFailedServers.erase(meshServer);
	}

	void IntraMeshService::forEachServer(boost::function<void (MeshServer::ptr)> visitor)
	{
		//randomize the order the servers are returned to better load balance things when 
		//more than one server has the asset we're looking for
		std::vector<MeshServer::ptr> randomizedServers;
		randomizedServers.reserve(_remoteServers.size());

		for (ServerMap::iterator i = _remoteServers.begin(); 
			i != _remoteServers.end(); ++i) {
			
				randomizedServers.push_back(i->second);
		}

		std::random_shuffle(randomizedServers.begin(), randomizedServers.end());

		BOOST_FOREACH(MeshServer::ptr server, randomizedServers)
		{
			visitor(server);
		}
	}

	MeshStorageBackend::ptr IntraMeshService::getMeshBackend()
	{
		return _meshStorageBackend;
	}

	bool IntraMeshService::connectionIsPending(const boost::asio::ip::tcp::endpoint& ep)
	{
		return _pendingServerConnections.find(ep) != _pendingServerConnections.end();
	}

	void IntraMeshService::getStatus(boost::shared_ptr<std::ostream> journal)
	{
		(*journal) << "-Intramesh Service" << std::endl;
		(*journal) << "  Connected Servers: " << _remoteServers.size() << std::endl;
		(*journal) << "-Intramesh Topology" << std::endl;

		for (ServerMap::iterator i = _remoteServers.begin(); 
			i != _remoteServers.end(); ++i) {
			
				(*journal) << "  " << i->second->getDescription() << std::endl;
		}
	}

	void IntraMeshService::addMeshRequestStat()
	{
		++_meshReqSinceLastPoll;
	}

	void IntraMeshService::addPositiveMeshRequestStat()
	{
		++_positiveRespSinceLastPoll;
	}

	void IntraMeshService::queryClientDisconnected(IntraMeshQueryClient::ptr client)
	{
		SAFELOG(AppLog::instance().out() 
			<< "[IWINTRAMESH] Query Client Disconnect: " << client->getEndpoint()
			<< std::endl);

		_connectedClients.erase(client);
	}

	void IntraMeshService::stop()
	{
		_stop = true;
	}
}
