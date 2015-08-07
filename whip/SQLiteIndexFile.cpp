#include "StdAfx.h"
#include "SQLiteIndexFile.h"


#include <boost/bind.hpp>
#include <boost/assert.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/ref.hpp>

#include "IndexFileError.h"
#include "SQLiteTransaction.h"
#include "SQLiteError.h"
#include "VFSCommon.h"

namespace iwvfs 
{
	const int SQLiteIndexFile::VERSION = 1;

	SQLiteIndexFile::SQLiteIndexFile(const boost::filesystem::path& absolutePath)
		: _filePath(absolutePath), _dbConn(new SQLiteConnection(_filePath))
	{

		//does the DB need to be initialized?
		DBResultSetPtr versionTableCount 
			= _dbConn->queryWithResults("SELECT COUNT(*) AS tbl_count "
										"FROM sqlite_master "
										"WHERE type = 'table' AND tbl_name='VFSVersions';" );
		BOOST_ASSERT(versionTableCount->size() == 1);
		
		DBRowPtr rowPtr = (*versionTableCount)[0];
		DBRow& row = *rowPtr;

		sqlite3_int64 tableCount = boost::any_cast<sqlite3_int64>(row["tbl_count"]);
		if (tableCount == 0) {
			//no version table means empty database, configure and set up for current version
			this->initializeDatabase();
		}
	}

	SQLiteIndexFile::~SQLiteIndexFile()
	{
		
	}

	void SQLiteIndexFile::initializeDatabase()
	{
		SQLiteTransaction trans(_dbConn);

		//first create our versioning table
		_dbConn->queryWithNoResults(
			"CREATE TABLE VFSVersions (version INTEGER PRIMARY KEY);");

		//then create the index table
		_dbConn->queryWithNoResults(
			"CREATE TABLE VFSDataIndex ("
				"asset_id CHARACTER(32) PRIMARY KEY, "
				"position BIGINT NOT NULL, "
				"type INTEGER NOT NULL, "
				"created_on DATETIME DEFAULT CURRENT_TIMESTAMP, "
				"deleted TINYINT NOT NULL "
			");");

		//finally insert the current version into the version table
		_dbConn->queryWithNoResults(
			"INSERT INTO VFSVersions(version) VALUES(" + 
			boost::lexical_cast<std::string>(SQLiteIndexFile::VERSION) +
			");");
		
		trans.commit();
	}

	boost::int64_t SQLiteIndexFile::findAssetPosition(const std::string& uuid)
	{
		DBResultSetPtr results 
			= _dbConn->queryWithResults(
				"SELECT * FROM VFSDataIndex WHERE asset_id = '" + uuid + "';");

		//exists?
		if (results->size() == 0) {
			return INVALID_POSITION;
		}

		//should only ever be one result
		BOOST_ASSERT(results->size() == 1);

		DBRowPtr rowPtr = (*results)[0];
		DBRow& row = *rowPtr;

		if (boost::any_cast<sqlite3_int64>(row["deleted"]) == 1) {
			return INVALID_POSITION;
		}

		sqlite3_int64 position = boost::any_cast<sqlite3_int64>(row["position"]);

		return boost::numeric_cast<boost::int64_t>(position);
	}

	void SQLiteIndexFile::recordAssetPosition(Asset::ptr asset, boost::int64_t position)
	{
		_dbConn->queryWithNoResults(
			"INSERT INTO VFSDataIndex(asset_id, position, type, deleted) "
			"VALUES("
				"'" + asset->getUUID() + "', " +
				boost::lexical_cast<std::string>(position) + ", " +
				boost::lexical_cast<std::string>((int)asset->getType()) + ", " +
				"0" +
			")"
		);
	}

	UuidListPtr SQLiteIndexFile::getContainedAssetIds()
	{
		UuidListPtr rowStorage(new UuidList());
		_dbConn->queryWithRowCallback("SELECT asset_id FROM VFSDataIndex", boost::bind(&SQLiteIndexFile::assetRowCallback, this, _1, rowStorage));

		return rowStorage;
	}

	void SQLiteIndexFile::assetRowCallback(sqlite3_stmt* stmt, UuidListPtr rowStorage)
	{
		rowStorage->push_back(kashmir::uuid::uuid_t(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))));
	}

	StringUuidListPtr SQLiteIndexFile::getContainedAssetIdStrings()
	{
		StringUuidListPtr rowStorage(new StringUuidList());
		_dbConn->queryWithRowCallback("SELECT asset_id FROM VFSDataIndex", boost::bind(&SQLiteIndexFile::assetStringRowCallback, this, _1, rowStorage));

		return rowStorage;
	}

	void SQLiteIndexFile::assetStringRowCallback(sqlite3_stmt* stmt, StringUuidListPtr rowStorage)
	{
		rowStorage->push_back(std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))));
	}
}
