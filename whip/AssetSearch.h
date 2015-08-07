#pragma once

#include "MeshServer.h"
#include "IntraMeshService.h"

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <set>
#include <string>

namespace iwintramesh
{
	class MeshStorageBackend;

	/**
	 * Stores the results of a search for assets.  This class will be owned by the 
	 * servers it is querying for the duration of the search
	 */
	class AssetSearch : public boost::enable_shared_from_this<AssetSearch>
	{
	public:
		typedef boost::shared_ptr<AssetSearch> ptr;
		typedef boost::weak_ptr<AssetSearch> wptr;

	private:
		static const short QUERY_TIMEOUT = 5;


		IntraMeshService& _meshService;
		boost::asio::io_service& _ioService;
		MeshStorageBackend* _meshBackend;
		std::string _uuid;
		boost::function<void(std::string, Asset::ptr)> _fullCallBack;
		boost::function<void(std::string, bool)> _testCallBack;
		
		bool _wasFound;
		boost::asio::deadline_timer _timeoutTimer;

		std::set<MeshServer::ptr> _queriedServers;
		MeshServer::ptr _foundServer;

		Asset::ptr _foundAsset;
		
		void initiateSearch(MeshServer::ptr server);

		void cancelTimeoutTimer();

		void cancelRemainingServers();

		void serverCompleted(MeshServer::ptr server);

		void onTimeout(const boost::system::error_code& error);

		/**
		 * Note calling this function will free the memory for THIS
		 */
		void markSearchComplete();

		void generateNegativeResponse();

		void onFullAssetRcvd(Asset::ptr asset);

	public:
		/**
		 * CTOR for a full asset search
		 */
		explicit 
		AssetSearch(IntraMeshService& meshService, boost::asio::io_service& ioService, 
			MeshStorageBackend* backend, const std::string& searchUUID, 
			boost::function<void(std::string, Asset::ptr)> callBack);

		/**
		 * CTOR for a test asset search
		 */
		explicit
		AssetSearch(IntraMeshService& meshService, boost::asio::io_service& ioService, 
		MeshStorageBackend* backend, const std::string& searchUUID, boost::function<void(std::string, bool)> callBack);

		virtual ~AssetSearch();

		/**
		 * Perform the search
		 */
		void performSearch();

		/**
		 * Was the asset found
		 */
		bool assetFound() const;

		/**
		 * Returns the uuid of the asset being searched
		 */
		const std::string& getSearchUUID() const;

		/** 
		 * Called by MeshServer instances when a search result is ready
		 */
		void onSearchResult(MeshServer::ptr server, std::string assetUUID, bool found);	

		/**
		 * Returns the asset that was found by the search
		 */
		Asset::ptr getFoundAsset();

		/**
		 * Returns a description of this search
		 */
		std::string getDescription();
	};
}