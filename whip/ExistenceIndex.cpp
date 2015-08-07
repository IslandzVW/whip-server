#include "StdAfx.h"
#include "ExistenceIndex.h"
#include "SQLiteConnection.h"
#include "AppLog.h"

#include <boost/bind.hpp>

namespace fs = boost::filesystem;

namespace iwvfs
{

ExistenceIndex::ExistenceIndex(const std::string& storageRoot)
{
	//find all index files in the subdirectories
	fs::path p(storageRoot);

	int i = 0;
	fs::directory_iterator end_itr; // default construction yields past-the-end
	for ( fs::directory_iterator itr( p );
		itr != end_itr;
		++itr )
	{
		if (fs::is_directory(itr->status()) ) {
			if (i % 100 == 0) {
				SAFELOG(AppLog::instance().out() << "[IWVFS] Processing index " << itr->path().string() << std::endl);
			}
			
			this->processDirectory(itr->path());
			++i;
		}
	}
}

ExistenceIndex::~ExistenceIndex()
{
}

void ExistenceIndex::processDirectory(boost::filesystem::path dir)
{
	fs::directory_iterator end_itr; 
	for ( fs::directory_iterator itr( dir );
		itr != end_itr;
		++itr )
	{
		if (! fs::is_directory(itr->status())) {
			fs::path file = itr->path();

			if (file.extension() == ".idx") {
				this->loadIndexFile(file);
			}
		}
	}
}

void ExistenceIndex::rowCallback(sqlite3_stmt* stmt)
{
	if (sqlite3_column_int(stmt, 1) == 0) {
		this->addNewId(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
	}
}

void ExistenceIndex::loadIndexFile(const boost::filesystem::path& path)
{
	SQLiteConnection conn(path.string());
	conn.queryWithRowCallback("SELECT asset_id, deleted FROM VFSDataIndex", boost::bind(&ExistenceIndex::rowCallback, this, _1));
}

bool ExistenceIndex::assetExists(const std::string& assetId)
{
	kashmir::uuid::uuid_t searchUUID(assetId.c_str());

	boost::mutex::scoped_lock lock(_lock);
	return _existingAssets.find(searchUUID) != _existingAssets.end();
}

void ExistenceIndex::addNewId(const std::string& assetId)
{
	kashmir::uuid::uuid_t newId(assetId.c_str());

	boost::mutex::scoped_lock lock(_lock);
	_existingAssets.insert(newId);
}

void ExistenceIndex::removeId(const kashmir::uuid::uuid_t& id)
{
	boost::mutex::scoped_lock lock(_lock);
	_existingAssets.erase(id);
}

}