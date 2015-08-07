#pragma once

#include "IIndexFileImplementation.h"
#include <boost/filesystem.hpp>

namespace iwvfs 
{
	/**
		Manages classes of index files and takes care of opening and closing the files
		depending on the perfornace considerations of the underlying storage system
	*/
	class IIndexFileManager
	{
	public:
		typedef boost::shared_ptr<IIndexFileManager> ptr;

	public:
		virtual ~IIndexFileManager();
		
		/**
		 *	Opens the given index file and returns a pointer to it
		 */
		virtual IIndexFileImplementation::ptr openIndexFile(const boost::filesystem::path& absolutePath) = 0;

		/**
		 * Forces the closure of the given index file
		 */
		virtual void forceCloseIndexFile(const boost::filesystem::path& absolutePath) = 0;

		/**
		 * Closes all open index files
		 */
		virtual void shutdown() = 0;
	};
}