#include "StdAfx.h"

#include "MeshStorageBackend.h"
#include "AssetSearch.h"
#include "AssetStorageError.h"
#include "IntraMeshService.h"

#include <boost/foreach.hpp>

namespace iwintramesh
{
	MeshStorageBackend::MeshStorageBackend(IntraMeshService& intraMeshService, 
		boost::asio::io_service& ioService)
	: _intraMeshService(intraMeshService), _ioService(ioService)
	{
		
	}

	MeshStorageBackend::~MeshStorageBackend()
	{

	}

	void MeshStorageBackend::getAsset(const std::string& uuid, boost::function<void(std::string, Asset::ptr)> callBack)
	{
		AssetSearch::ptr search(new AssetSearch(_intraMeshService, _ioService, this, uuid, callBack));
		_activeSearches.insert(search);
		search->performSearch();
	}

	void MeshStorageBackend::searchComplete(AssetSearch::ptr search)
	{
		if (_assetCache) {
			_assetCache->inform(search->getFoundAsset());
		}

		_activeSearches.erase(search);
	}

	void MeshStorageBackend::storeAsset(Asset::ptr asset)
	{
		throw AssetStorageError("Can not store asset to mesh storage. Feature not implemented");
	}

	void MeshStorageBackend::assetExists(const std::string& uuid, boost::function<void(std::string, bool)> callBack)
	{
		AssetSearch::ptr search(new AssetSearch(_intraMeshService, _ioService, this, uuid, callBack));
		_activeSearches.insert(search);
		search->performSearch();
	}

	void MeshStorageBackend::purgeAsset(const std::string& uuid)
	{
		throw AssetStorageError("Can not puge asset from mesh storage. Feature not implemented");
	}

	void MeshStorageBackend::setCache(AssetCache::ptr assetCache)
	{
		_assetCache = assetCache;
	}

	void MeshStorageBackend::getStatus(boost::shared_ptr<std::ostream> journal)
	{
		(*journal) << "-Mesh Backend" << std::endl;
		(*journal) << "  Search Queue Size: " << _activeSearches.size() << std::endl;
		(*journal) << "-Active Mesh Searches" << std::endl;

		BOOST_FOREACH(AssetSearch::ptr search, _activeSearches)
		{
			(*journal) << "  " << search->getDescription() << std::endl;
		}

		_intraMeshService.getStatus(journal);
	}
}