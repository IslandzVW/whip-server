// tests.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <boost/filesystem.hpp>
#include <boost/bind.hpp>
#include "SQLiteConnection.h"
#include "kashmir/uuid.h"

#include <string>
#include <iostream>
#include <set>

namespace fs = boost::filesystem;

std::set<kashmir::uuid::uuid_t> uuids;

void addId(sqlite3_stmt* stmt)
{
	uuids.insert(kashmir::uuid::uuid_t(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))));
	
	if (uuids.size() % 1000 == 0)
	{
		std::cout << uuids.size() << std::endl;
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	using namespace iwvfs;

	if (argc < 2) return 2;

	//find all index files in the directory and load the UUIDs
	std::string pathStr((char*)(argv[1]));
	fs::path p(pathStr);

	fs::directory_iterator end_itr; // default construction yields past-the-end
	for ( fs::directory_iterator itr( p );
		itr != end_itr;
		++itr )
	{
		if (! fs::is_directory(itr->status()) )
		{
			SQLiteConnection conn(itr->path().string());
			conn.queryWithRowCallback("SELECT asset_id FROM VFSDataIndex", boost::bind(addId, _1));
		}
	}

	return 0;
}

