#include "StdAfx.h"

#include "AssetCache.h"
#include "AppLog.h"

AssetCache::AssetCache(unsigned long long maxCacheSize, IAsyncStorageBackend::ptr persistentStorage, 
					   boost::asio::io_service& ioService)
: _maxCacheSize(maxCacheSize), _persistentStorage(persistentStorage), _cache(maxCacheSize), 
	_ioService(ioService), _cacheHits(0), _cacheMisses(0)
{
	SAFELOG(AppLog::instance().out() << "Asset cache enabled (" << maxCacheSize / 1000000 << " MB)" << std::endl);
}

AssetCache::~AssetCache()
{
}


void AssetCache::inform(Asset::ptr asset)
{
	if (asset) {
		_cache.insert(asset->getUUID(), asset);
	}
}

void AssetCache::getAsset(const std::string& uuid, AsyncGetAssetCallback callBack)
{
	//test the cache first to see if we have the asset
	Asset::ptr asset = _cache.fetch(uuid);
	if (asset) {
		//we have the asset in memory, return that
		_ioService.post(boost::bind(callBack, asset, true, AssetStorageError::ptr()));
		++_cacheHits;

	} else {
		//load from persistent storage
		_persistentStorage->getAsset(uuid, boost::bind(&AssetCache::asyncGetCompleted, this, _1, _2, _3, SF_NONE, callBack));
		++_cacheMisses;
	}
}

void AssetCache::getAsset(const std::string& uuid, unsigned int flags, AsyncGetAssetCallback callBack)
{
	//test the cache first to see if we have the asset
	Asset::ptr asset = _cache.fetch(uuid);
	if (asset) {
		//we have the asset in memory, return that
		_ioService.post(boost::bind(callBack, asset, true, AssetStorageError::ptr()));
		++_cacheHits;

	} else {
		//load from persistent storage
		_persistentStorage->getAsset(uuid, boost::bind(&AssetCache::asyncGetCompleted, this, _1, _2, _3, flags, callBack));
		++_cacheMisses;
	}
}

void AssetCache::asyncGetCompleted(Asset::ptr asset, bool success, AssetStorageError::ptr error, unsigned int flags, AsyncGetAssetCallback callBack)
{
	if (asset && success && ((flags & SF_DONTCACHE) == 0)) {
		_cache.insert(asset->getUUID(), asset);
	}

	callBack(asset, success, error);
}

void AssetCache::storeAsset(Asset::ptr asset, AsyncStoreAssetCallback callBack)
{
	_persistentStorage->storeAsset(asset, boost::bind(&AssetCache::asyncPutCompleted, this, asset, _1, _2, callBack));
}

void AssetCache::asyncPutCompleted(Asset::ptr asset, bool success, AssetStorageError::ptr error, AsyncStoreAssetCallback callBack)
{
	if (success) {
		_cache.insert(asset->getUUID(), asset);
	}

	callBack(success, error);
}

void AssetCache::purgeAsset(const std::string& uuid, AsyncAssetPurgeCallback callBack)
{
	
}

bool AssetCache::assetExists(const std::string& uuid)
{
	return _cache.exists(uuid) || _persistentStorage->assetExists(uuid);
}

void AssetCache::beginPurgeLocals()
{
	//we dont do anything here but our sotrage backend may
	_persistentStorage->beginPurgeLocals();
}

void AssetCache::getStatus(boost::shared_ptr<std::ostream> journal, boost::function<void()> completedCallback)
{
	(*journal) << "-Asset Cache" << std::endl;
	(*journal) << "  Cache max size (MB): " << _maxCacheSize / 1000000 << std::endl;
	(*journal) << "  Cache item count: " << _cache.size() << std::endl;
	(*journal) << "  Cache hits/miss: " << _cacheHits << "/" << _cacheMisses << std::endl;

	unsigned long long totalRequests = _cacheHits + _cacheMisses;
	if (totalRequests > 0) {
		unsigned long long cacheMissPercentage = (unsigned long long)(((float)_cacheMisses / totalRequests) * 100.0f);
		(*journal) << "  Cache hit%: " << 100 - cacheMissPercentage << std::endl;
	}

	_persistentStorage->getStatus(journal, completedCallback);
}

void AssetCache::getStoredAssetIDsWithPrefix(std::string prefix, boost::shared_ptr<std::ostream> journal, boost::function<void()> completedCallback)
{
	_persistentStorage->getStoredAssetIDsWithPrefix(prefix, journal, completedCallback);
}

void AssetCache::shutdown()
{
	_persistentStorage->shutdown();
}