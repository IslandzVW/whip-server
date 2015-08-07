#include "StdAfx.h"
#include "DatabaseSet.h"

#include "AssetStorageError.h"

namespace iwvfs
{
	DatabaseSet::DatabaseSet(const boost::filesystem::path& databaseRoot)
		:	_globalDB(new VFSDatabase("globals", databaseRoot)),
			_localDB(new VFSDatabase("locals", databaseRoot))
	{
		
	}

	DatabaseSet::~DatabaseSet()
	{
	}

	Asset::ptr DatabaseSet::getAsset(const std::string& uuid)
	{
		Asset::ptr globalAsset(_globalDB->getAsset(uuid));
		if (globalAsset) {
			return globalAsset;
		}

		Asset::ptr localAsset(_localDB->getAsset(uuid));
		if (localAsset) {
			return localAsset;
		}
		
		return Asset::ptr();
	}

	void DatabaseSet::storeAsset(Asset::ptr asset)
	{
		if (asset->isLocal()) {
			_localDB->storeAsset(asset);

		} else {
			_globalDB->storeAsset(asset);

		}
	}

	bool DatabaseSet::assetExists(const std::string& uuid)
	{
		if (_globalDB->assetExists(uuid)) {
			return true;
		}

		if (_localDB->assetExists(uuid)) {
			return true;
		}

		return false;
	}

	void DatabaseSet::purgeAsset(const std::string& uuid)
	{
		if (_globalDB->assetExists(uuid)) {
			_globalDB->purgeAsset(uuid);
		} else if (_localDB->assetExists(uuid)) {
			_localDB->purgeAsset(uuid);
		} else {
			throw AssetStorageError("[IWVFS] Could not purge asset " + uuid + ", asset not found in database storage"); 
		}
	}
}