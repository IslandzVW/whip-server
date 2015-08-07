#pragma once

#include "VFSCommon.h"
#include "IIndexFileManager.h"
#include "kashmir/uuid.h"

#include <boost/shared_ptr.hpp>
#include <boost/cstdint.hpp>
#include <vector>

namespace iwvfs
{
	/**
		The index portion of the InWorldz VFS database
	*/
	class IndexFile
	{
	public:
		typedef boost::shared_ptr<IndexFile> ptr;
		
		static void SetIndexFileManager(IIndexFileManager::ptr fileManager);
		static IIndexFileManager::ptr GetIndexFileManager();

	private:
		static IIndexFileManager::ptr _FileManager;
		boost::filesystem::path _filePath;
		IIndexFileImplementation::ptr _impl;

	public:
		/**
			Creates a new index file resource.  Does not actually open
			the index file until a call is made that requires the data
		*/
		IndexFile(const boost::filesystem::path& filePath);
		virtual ~IndexFile();
		
		/**
			Finds the location in the database file of the given
			asset if it exists in the index, otherwise returns
			INVALID_POSITION
		*/
		boost::int64_t findAssetPosition(const std::string& uuid);
		
		/**
			Inserts the asset location into the index
		*/
		void recordAssetPosition(Asset::ptr asset, boost::int64_t position);

		/**
		 * Returns all asset ids contained in this file
		 */
		UuidListPtr getContainedAssetIds();

		/**
		 * Returns all asset ids contained in this file as strings
		 */
		StringUuidListPtr getContainedAssetIdStrings();
	};

}