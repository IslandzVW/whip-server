#pragma once

#include "Asset.h"
#include "AssetStorageError.h"
#include "AsyncStorageTypes.h"

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <iosfwd>
#include <string>

enum StorageFlags
{
	SF_NONE = 0,
	SF_DONTCACHE = (1 << 0)
};

/**
 * Interface to an async storage backend
 */
class IAsyncStorageBackend
{
public:
	typedef boost::shared_ptr<IAsyncStorageBackend> ptr;

public:
	/**
	 * Returns the asset from the storage backend or a null reference if the asset doesn't exist
	 */
	virtual void getAsset(const std::string& uuid, AsyncGetAssetCallback callBack) = 0;

	/**
	 * Returns the asset from the storage backend or a null reference if the asset doesn't exist
	 */
	virtual void getAsset(const std::string& uuid, unsigned int flags, AsyncGetAssetCallback callBack) = 0;

	/**
	 * Saves an asset into the storage backend
	 */
	virtual void storeAsset(Asset::ptr asset, AsyncStoreAssetCallback callBack) = 0;

	/**
	 * Returns true if the asset is stored in the backend, false if not.
	 * This call is sync because it uses the in memory existence index
	 */
	virtual bool assetExists(const std::string& uuid) = 0;

	/**
	 * Deletes the asset from the storage backend
	 */
	virtual void purgeAsset(const std::string& uuid, AsyncAssetPurgeCallback callBack) = 0;

	/**
	 * Begins the process of purging all local assets from this server
	 */
	virtual void beginPurgeLocals() = 0;

	/**
	 * Records the status of the storage component
	 */
	virtual void getStatus(boost::shared_ptr<std::ostream> journal, boost::function<void()> completedCallback) = 0;

	/**
	 * Returns all asset IDs this backend contains with the given hex prefix
	 */
	virtual void getStoredAssetIDsWithPrefix(std::string prefix, boost::shared_ptr<std::ostream> journal, boost::function<void()> completedCallback) = 0;

	/**
	 * Performs a clean shutdown of this backend
	 */
	virtual void shutdown() = 0;

	IAsyncStorageBackend();
	virtual ~IAsyncStorageBackend();
};
