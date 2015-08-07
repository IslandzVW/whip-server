#pragma once
#include "IIndexFileManager.h"

#include <boost/shared_ptr.hpp>

#include "lru_cache.h"
#include "SQLiteIndexFile.h"

namespace iwvfs 
{
	/**
		Manages open sqlite files to serve as indexes for datafiles
	*/
	class SQLiteIndexFileManager : public IIndexFileManager
	{
	public:
		typedef boost::shared_ptr<SQLiteIndexFileManager> ptr;

	private:
		static const short MAX_OPEN_FILES = 512;

		LRUCache<std::string, SQLiteIndexFile::ptr> _openFiles;

	public:
		SQLiteIndexFileManager();
		virtual ~SQLiteIndexFileManager();

		IIndexFileImplementation::ptr openIndexFile(const boost::filesystem::path& indexFilePath);
		void forceCloseIndexFile(const boost::filesystem::path& indexFilePath);

		void shutdown();
	};
}