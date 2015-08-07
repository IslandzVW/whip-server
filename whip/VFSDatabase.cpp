#include "StdAfx.h"
#include "VFSDatabase.h"

#include <boost/filesystem/fstream.hpp>
#include <stdexcept>

#include "AppLog.h"
#include "AssetStorageError.h"
#include "Settings.h"

namespace fs = boost::filesystem;

namespace iwvfs
{
	VFSDatabase::VFSDatabase(const std::string& fileNameBase, const fs::path& path)
		: _fileNameBase(fileNameBase), _path(path)
	{
		//make sure the path where the files will go exists
		if (! fs::exists(path)) {
			throw AssetStorageError("Could not mount VFS database: " + path.string() + " path was not found", true);
		}

		_indexFile.reset(new IndexFile(path / fs::path(fileNameBase + ".idx")));
		_dataFile.reset(new DataFile(path / fs::path(fileNameBase + ".data")));
	}

	VFSDatabase::~VFSDatabase()
	{
	}

	Asset::ptr VFSDatabase::getAsset(const std::string& uuid)
	{
		boost::int64_t assetPosition = _indexFile->findAssetPosition(uuid);
		if (assetPosition == INVALID_POSITION) {
			return Asset::ptr();
		}
		
		//position exists in the index.  attempt retrieval from the database
		Asset::ptr asset = _dataFile->getAssetAtPosition(assetPosition);
		return asset;
	}

	void VFSDatabase::storeAsset(Asset::ptr asset)
	{
		if (! this->assetExists(asset->getUUID())) {
			boost::int64_t position = _dataFile->writeNewAsset(asset);
			_indexFile->recordAssetPosition(asset, position);

		} else {
			throw AssetStorageError("Unable to store asset " + asset->getUUID() + ", asset already exists", true);
		}
	}

	bool VFSDatabase::assetExists(const std::string& uuid)
	{
		boost::int64_t assetPosition = _indexFile->findAssetPosition(uuid);
		if (assetPosition != INVALID_POSITION) {
			return true;
		} else {
			return false;
		}
	}

	void VFSDatabase::purgeAsset(const std::string& uuid)
	{
		throw AssetStorageError("[IWVFS] Purge is not yet implemented for VFS");
	}

}