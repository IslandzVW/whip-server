#pragma once

#include <boost/shared_ptr.hpp>
#include "Asset.h"
#include "IndexFile.h"
#include "DataFile.h"

namespace iwvfs
{
	/**
		Represents a single vfs database that consists of an index file and a 
		datafile.
	*/
	class VFSDatabase
	{
	public:
		typedef boost::shared_ptr<VFSDatabase> ptr;

	private:
		std::string _fileNameBase;
		boost::filesystem::path _path;
		
		IndexFile::ptr _indexFile;
		DataFile::ptr _dataFile;


	public:
		/**
			Creates an access point to a vfs database
		*/
		VFSDatabase(const std::string& fileNameBase, const boost::filesystem::path& path);
		virtual ~VFSDatabase();

		Asset::ptr getAsset(const std::string& uuid);
		void storeAsset(Asset::ptr asset);
		bool assetExists(const std::string& uuid);
		void purgeAsset(const std::string& uuid);
	};

}