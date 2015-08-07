
#pragma once

#include <boost/filesystem/path.hpp>

#include "IIndexFileImplementation.h"
#include "SQLiteConnection.h"

namespace iwvfs 
{
	/**
		Uses an sqlite database to keep tract of the file locations of
		an asset set
	*/
	class SQLiteIndexFile : public IIndexFileImplementation
	{
	public:
		const static int VERSION;

	private:
		boost::filesystem::path _filePath;
		SQLiteConnection::ptr _dbConn;

		void initializeDatabase();

		void assetRowCallback(sqlite3_stmt* stmt, UuidListPtr rowStorage);
		void assetStringRowCallback(sqlite3_stmt* stmt, StringUuidListPtr rowStorage);

	public:
		SQLiteIndexFile(const boost::filesystem::path& absolutePath);
		virtual ~SQLiteIndexFile();

		virtual boost::int64_t findAssetPosition(const std::string& uuid);
		virtual void recordAssetPosition(Asset::ptr asset, boost::int64_t position);
		virtual UuidListPtr getContainedAssetIds();
		virtual StringUuidListPtr getContainedAssetIdStrings();
	};
}