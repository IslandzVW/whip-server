#include "StdAfx.h"
#include "AssetSearch.h"
#include "AppLog.h"
#include "MeshStorageBackend.h"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <sstream>

namespace iwintramesh
{
	AssetSearch::AssetSearch(IntraMeshService& meshService, boost::asio::io_service& ioService,
		MeshStorageBackend* meshBackend, const std::string& searchUUID, 
		boost::function<void(std::string, Asset::ptr)> callBack)
	: _meshService(meshService), _ioService(ioService), _meshBackend(meshBackend), 
		_uuid(searchUUID), _wasFound(false), _timeoutTimer(_ioService)
	{
		_fullCallBack = callBack;
	}

	AssetSearch::AssetSearch(IntraMeshService& meshService, boost::asio::io_service& ioService,
		MeshStorageBackend* meshBackend, const std::string& searchUUID, 
		boost::function<void(std::string, bool)> callBack)
	: _meshService(meshService), _ioService(ioService), _meshBackend(meshBackend), 
		_uuid(searchUUID), _wasFound(false), _timeoutTimer(_ioService)
	{
		_testCallBack = callBack;
	}

	AssetSearch::~AssetSearch()
	{
	}

	void AssetSearch::onSearchResult(MeshServer::ptr server, std::string assetUUID, bool found)
	{
		if (_wasFound) {
			SAFELOG(AppLog::instance().out() 
				<< "[IWINTRAMESH] Warning: BUG: asset already found but search result callback activated"
				<< std::endl);
		}

		if (assetUUID != _uuid) {
			SAFELOG(AppLog::instance().out() 
				<< "[IWINTRAMESH] Warning: BUG: Asset UUID doesnt match asset searched for"
				<< std::endl);
		}

		this->serverCompleted(server);

		if (found) {
			this->cancelTimeoutTimer();
			this->cancelRemainingServers();

			//if this is just a test, we're done here
			if (_testCallBack) {
				_testCallBack(assetUUID, true);
				this->markSearchComplete();

			} else if (_fullCallBack) {
				//else we need to actually grab the asset data
				_foundServer = server;
				_wasFound = true;
				
				_foundServer->getAsset(_uuid, boost::bind(&AssetSearch::onFullAssetRcvd, this, _1));
			}

		} else {
			if (_queriedServers.size() == 0) {
				this->cancelTimeoutTimer();
				this->generateNegativeResponse();
				this->markSearchComplete();
			}
		}
	}

	void AssetSearch::cancelRemainingServers()
	{
		BOOST_FOREACH(MeshServer::ptr server, _queriedServers)
		{
			server->cancelSearch(shared_from_this());
		}

		_queriedServers.clear();
	}

	void AssetSearch::serverCompleted(MeshServer::ptr server)
	{
		_queriedServers.erase(server);
	}

	void AssetSearch::cancelTimeoutTimer()
	{
		_timeoutTimer.cancel();
	}

	void AssetSearch::initiateSearch(MeshServer::ptr server)
	{
		if (!server->isTimedOut() && (server->getHeartbeatFlags() & IntraMeshMsg::HB_READABLE)) {
			_queriedServers.insert(server);
			server->search(shared_from_this());
		}
	}

	void AssetSearch::performSearch()
	{
		_meshService.forEachServer(boost::bind(&AssetSearch::initiateSearch, this, _1));

		if (_queriedServers.size() == 0) {
			//nothing to do
			this->generateNegativeResponse();
			this->markSearchComplete();
			return;
		}

		//also set a timer to cancel the pending ops incase of timeouts
		_timeoutTimer.expires_from_now(boost::posix_time::seconds(QUERY_TIMEOUT));
		_timeoutTimer.async_wait(boost::bind(&AssetSearch::onTimeout, this,
			boost::asio::placeholders::error));
	}

	void AssetSearch::markSearchComplete()
	{
		_meshBackend->searchComplete(shared_from_this());
	}

	void AssetSearch::generateNegativeResponse()
	{
		if (_fullCallBack) {
			_fullCallBack(_uuid, Asset::ptr());
		} 

		if (_testCallBack) {
			_testCallBack(_uuid, false);
		}
	}

	void AssetSearch::onTimeout(const boost::system::error_code& error)
	{
		if (!error && ! _wasFound) {
			std::stringstream servers;

			BOOST_FOREACH(MeshServer::ptr server, _queriedServers)
			{
				servers << server->getQueryEndpoint() << ", ";
			}

			SAFELOG(AppLog::instance().out() 
				<< "[IWINTRAMESH] Warning: Timeout while waiting for response from intramesh for asset "
				<< _uuid
				<< " canceled: " << servers.str()
				<< std::endl);

			this->cancelRemainingServers();
			this->generateNegativeResponse();
			this->markSearchComplete();
		}
	}

	bool AssetSearch::assetFound() const
	{
		return _wasFound;
	}

	const std::string& AssetSearch::getSearchUUID() const
	{
		return _uuid;
	}

	void AssetSearch::onFullAssetRcvd(Asset::ptr asset)
	{
		if (_fullCallBack) {
			_foundAsset = asset;
			_fullCallBack(_uuid, asset);
		}

		this->markSearchComplete();
	}

	Asset::ptr AssetSearch::getFoundAsset()
	{
		return _foundAsset;
	}

	std::string AssetSearch::getDescription()
	{
		std::stringstream builder;

		std::string status;
		if (_wasFound) {
			std::stringstream foundmsg;
			foundmsg << "FOUND ";
			foundmsg << _foundServer->getQueryEndpoint();

			status = foundmsg.str();

		} else {
			status = "SEARCHING";
		}

		builder 
			<< _uuid << " " 
			<< status << ", "
			<< "Servers Remaining: ";

		BOOST_FOREACH(MeshServer::ptr server, _queriedServers)
		{
			builder << server->getQueryEndpoint() << ", ";
		}

		return builder.str();
	}
}
