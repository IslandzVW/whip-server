#include "StdAfx.h"
#include "DataFile.h"

#include <stdexcept>
#include <boost/filesystem/fstream.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/cstdint.hpp>

#include "AssetStorageError.h"
#include "IDataFile.h"

#ifdef _WIN32
	#include "WinDataFileImpl.h"
	namespace iwvfs {
		typedef WinDataFileImpl DataFileImpl;
	}
#endif

#ifdef __linux__
	#include "PosixDataFileImpl.h"
	namespace iwvfs {
		typedef PosixDataFileImpl DataFileImpl;
	}
#endif

namespace fs = boost::filesystem;
using namespace std;

namespace iwvfs
{

	DataFile::DataFile(const boost::filesystem::path& filePath)
		: _filePath(filePath)
	{
	}

	DataFile::~DataFile()
	{
	}

	Asset::ptr DataFile::getAssetAtPosition(boost::int64_t position)
	{
		IDataFile::ptr dataFile(new DataFileImpl());
		dataFile->open(_filePath, IDataFile::Read);

		dataFile->seekg(position);

		//read the asset size (first 4 bytes) stored in network byte order
		boost::uint32_t assetSize;
		dataFile->read((char*)&assetSize, 4);
		assetSize = ntohl(assetSize);

		//read in the asset
		whip::byte_array_ptr data(new whip::byte_array());
		data->resize(assetSize);
		dataFile->read((char*) &((*data)[0]), assetSize);
		dataFile->close();

		//return the built asset
		Asset::ptr foundAsset(new Asset(data));
		return foundAsset;
	}

	boost::int64_t DataFile::writeNewAsset(Asset::ptr newAsset)
	{
		bool fileExisted = fs::exists(_filePath);
		
		IDataFile::ptr dataFile(new DataFileImpl());
		dataFile->open(_filePath, IDataFile::Write);

		//grab the position for the indexer
		boost::uint64_t pos;
		if (! fileExisted) {
			dataFile->write("IWZDDB01", 8);
			pos = 8;
		} else {
			pos = dataFile->size();
			dataFile->seekp(pos);
		}

		whip::byte_array_ptr assetData(newAsset->getData());
		
		//write out the size in network byte order
		boost::uint32_t assetSize = boost::numeric_cast<boost::uint32_t>(assetData->size());
		assetSize = htonl(assetSize);
		dataFile->write((char*)&assetSize, 4); 

		//write out the contents
		dataFile->write((char*) &((*assetData)[0]), boost::numeric_cast<boost::uint32_t>(assetData->size()));

		dataFile->close();
		return pos;
	}

}