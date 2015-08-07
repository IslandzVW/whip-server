#pragma once

#include <iosfwd>
#include <boost/filesystem/path.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/cstdint.hpp>

#include "Asset.h"

namespace iwvfs
{
	/**
		Represents the large object storage file
	*/
	class DataFile
	{
	public:
		typedef boost::shared_ptr<DataFile> ptr;
	
	private:
		boost::filesystem::path _filePath;

	public:
		/**
			Sets up a datafile at the path given
		*/
		DataFile(const boost::filesystem::path& filePath);
		virtual ~DataFile();

		/**
			Returns the asset at the given position in the data file
		*/
		Asset::ptr getAssetAtPosition(boost::int64_t position);

		/**
			Writes a new asset to the end of the file and returns
			the position for the asset
		*/
		boost::int64_t writeNewAsset(Asset::ptr newAsset);
	};

}