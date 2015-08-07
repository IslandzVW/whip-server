#pragma once

#include <string>

#include "Asset.h"
#include "VFSDatabase.h"
#include <boost/filesystem.hpp>

namespace iwvfs
{
	/**
		Represents a set of databases that would be appropritate
		to read and write for the given asset id.  Currently the
		only databases that will be opened for this operation
		are a local and a global database
	*/
	class DatabaseSet
	{
	public:
		typedef boost::shared_ptr<DatabaseSet> ptr;

	private:
		VFSDatabase::ptr _globalDB;
		VFSDatabase::ptr _localDB;

	public:
		/**
			Prepares a database set for the given assetid
		*/
		DatabaseSet(const boost::filesystem::path& databaseRoot);
		virtual ~DatabaseSet();

		Asset::ptr getAsset(const std::string& uuid);
		void storeAsset(Asset::ptr asset);
		bool assetExists(const std::string& uuid);
		void purgeAsset(const std::string& uuid);
	};
}