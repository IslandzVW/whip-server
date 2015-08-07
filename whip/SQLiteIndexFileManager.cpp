#include "StdAfx.h"
#include "SQLiteIndexFileManager.h"

namespace iwvfs
{
	SQLiteIndexFileManager::SQLiteIndexFileManager()
	: _openFiles(SQLiteIndexFileManager::MAX_OPEN_FILES)
	{
	}

	SQLiteIndexFileManager::~SQLiteIndexFileManager()
	{
	}

	IIndexFileImplementation::ptr SQLiteIndexFileManager::openIndexFile(const boost::filesystem::path& indexFilePath)
	{
		//do we already have a matching open file?
		std::string strPath(indexFilePath.string());
		IIndexFileImplementation::ptr indexFile(_openFiles.fetch(strPath));

		if (! indexFile) {
			indexFile.reset(new SQLiteIndexFile(indexFilePath));
			_openFiles.insert(strPath, indexFile);
		} 

		return indexFile;
	}

	void SQLiteIndexFileManager::forceCloseIndexFile(const boost::filesystem::path& indexFilePath)
	{
		//do we already have a matching open file?
		std::string strPath(indexFilePath.string());
		IIndexFileImplementation::ptr indexFile(_openFiles.fetch(strPath));

		if (indexFile) {
			_openFiles.remove(strPath);
		}
	}

	void SQLiteIndexFileManager::shutdown()
	{
		_openFiles.clear();
	}
}