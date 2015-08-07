#pragma once

#include "Asset.h"
#include "AssetSizeCalculator.h"
#include "lru_cache.h"
#include "IStorageBackend.h"
#include "IAsyncStorageBackend.h"

#include <boost/asio.hpp>

/**
 * Sits between the storage backend and the server caching assets
 */
class AssetCache : public IAsyncStorageBackend
{
public:
	typedef boost::shared_ptr<AssetCache> ptr;

private:
	unsigned long long _maxCacheSize;
	IAsyncStorageBackend::ptr _persistentStorage;

	LRUCache<std::string, Asset::ptr, AssetSizeCalculator> _cache;

	boost::asio::io_service& _ioService;

	unsigned long long _cacheHits;
	unsigned long long _cacheMisses;

	void asyncGetCompleted(Asset::ptr asset, bool success, AssetStorageError::ptr error, unsigned int flags, AsyncGetAssetCallback callBack);
	void asyncPutCompleted(Asset::ptr asset, bool wasError, AssetStorageError::ptr error, AsyncStoreAssetCallback callBack);

public:
	/**
	 * Creates a new assetcache instance
	 *
	 * @param maxCacheSize The maximum size in megabytes for the cache
	 * @param persistentStorage The persistant storage provider the cache is being serviced from
	*/
	AssetCache(unsigned long long maxCacheSize, IAsyncStorageBackend::ptr persistentStorage, 
		boost::asio::io_service& ioService);
	virtual ~AssetCache();


	/**
	 * Informs the cache of a new asset from an external source
	 */
	void inform(Asset::ptr asset);

	//Async
	/**
	 * Returns the asset from the storage backend or a null reference if the asset doesn't exist
	 */
	virtual void getAsset(const std::string& uuid, AsyncGetAssetCallback callBack);

	/**
	 * Returns the asset from the storage backend or a null reference if the asset doesn't exist
	 */
	virtual void getAsset(const std::string& uuid, unsigned int flags, AsyncGetAssetCallback callBack);

	/**
	 * Saves an asset into the storage backend
	 */
	virtual void storeAsset(Asset::ptr asset, AsyncStoreAssetCallback callBack);

	/**
	 * Deletes the asset from the storage backend
	 */
	virtual void purgeAsset(const std::string& uuid, AsyncAssetPurgeCallback callBack);

	/**
	 * Returns true if the asset is stored in the backend, false if not.
	 * This call is sync because it uses the in memory existence index
	 */
	virtual bool assetExists(const std::string& uuid);

	/**
	 * The asset cache never purges
	 */
	virtual void beginPurgeLocals();

	/**
	 * Writes the cache status and calls into the disk backend for it's status
	 */
	virtual void getStatus(boost::shared_ptr<std::ostream> journal, boost::function<void()> completedCallback);

	virtual void getStoredAssetIDsWithPrefix(std::string prefix, boost::shared_ptr<std::ostream> journal, boost::function<void()> completedCallback);

	/**
	 * Calls shutdown on child backends
	 */
	virtual void shutdown();
};
