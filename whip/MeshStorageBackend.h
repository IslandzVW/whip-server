#pragma once

#include "IStorageBackend.h"
#include "AssetSearch.h"
#include "AssetCache.h"

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

#include <set>

namespace iwintramesh
{
	class IntraMeshService;

	/**
	 * Storage backend that uses a connection to a mesh server to provide 
	 * data results
	 */
	class MeshStorageBackend
	{
	public:
		typedef boost::shared_ptr<MeshStorageBackend> ptr;

	private:
		IntraMeshService& _intraMeshService;
		boost::asio::io_service& _ioService;
		AssetCache::ptr _assetCache;

		std::set<AssetSearch::ptr> _activeSearches;
		
	public:
		/**
		 * CTOR
		 */
		MeshStorageBackend(IntraMeshService& intraMeshService,
			boost::asio::io_service& ioService);

		virtual ~MeshStorageBackend();

		virtual void getAsset(const std::string& uuid, boost::function<void(std::string, Asset::ptr)> callBack);

		virtual void storeAsset(Asset::ptr asset);

		virtual void assetExists(const std::string& uuid, boost::function<void(std::string, bool)>);

		virtual void purgeAsset(const std::string& uuid);

		void searchComplete(AssetSearch::ptr search);

		void setCache(AssetCache::ptr assetCache);

		void getStatus(boost::shared_ptr<std::ostream> journal); 
	};
}