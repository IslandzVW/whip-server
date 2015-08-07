#pragma once

#include <boost/shared_ptr.hpp>
#include "Asset.h"

/**
 * Interface to an asset storage backend
 */
class IStorageBackend
{
public:
	typedef boost::shared_ptr<IStorageBackend> ptr;

public:
	/**
	 * Returns the asset from the storage backend or a null reference if the asset doesn't exist
	 */
	virtual Asset::ptr getAsset(const std::string& uuid) = 0;

	/**
	 * Saves an asset into the storage backend
	 */
	virtual void storeAsset(Asset::ptr asset) = 0;

	/**
	 * Returns true if the asset is stored in the backend, false if not.
	 * Note that this function should not be used by external code as a test 
	 * preceeding other calls to this interface as that would slow the 
	 * performance of some backends.
	 */
	virtual bool assetExists(const std::string& uuid) = 0;

	/**
	 * Deletes the asset from the storage backend
	 */
	virtual void purgeAsset(const std::string& uuid) = 0;


	IStorageBackend();
	virtual ~IStorageBackend();
};
