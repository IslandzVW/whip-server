#include "StdAfx.h"
#include "IndexFile.h"

#include <fstream>
#include <boost/numeric/conversion/cast.hpp>

#include "AssetStorageError.h"

namespace fs = boost::filesystem;
using namespace std;

namespace iwvfs
{
	IIndexFileManager::ptr IndexFile::_FileManager;

	void IndexFile::SetIndexFileManager(IIndexFileManager::ptr fileManager)
	{
		_FileManager = fileManager;
	}

	IIndexFileManager::ptr IndexFile::GetIndexFileManager()
	{
		return _FileManager;
	}

	IndexFile::IndexFile(const boost::filesystem::path& filePath)
		: _filePath(filePath), _impl(_FileManager->openIndexFile(filePath))
	{
		
	}

	IndexFile::~IndexFile()
	{
	}

	boost::int64_t IndexFile::findAssetPosition(const std::string& uuid)
	{
		return _impl->findAssetPosition(uuid);
	}
	
	void IndexFile::recordAssetPosition(Asset::ptr asset, boost::int64_t position)
	{
		_impl->recordAssetPosition(asset, position);
	}

	UuidListPtr IndexFile::getContainedAssetIds()
	{
		return _impl->getContainedAssetIds();
	}

	StringUuidListPtr IndexFile::getContainedAssetIdStrings()
	{
		return _impl->getContainedAssetIdStrings();
	}
}