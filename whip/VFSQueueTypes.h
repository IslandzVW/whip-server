#pragma once

#include "AsyncStorageTypes.h"

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iosfwd>


namespace iwvfs
{
	class VFSBackend;

	/**
	 * A queued request to the asset services
	 */
	class AssetRequest
	{
	public:
		typedef boost::shared_ptr<AssetRequest> ptr;

		/**
		 * The type of request being made
		 */
		enum AssetRequestType
		{
			AR_GET,
			AR_PUT,
			AR_PURGE,
			AR_PURGE_LOCALS,
			AR_DELETE_LOCAL_STORAGE,
			AR_STATUS_GET,
			AR_STORED_ASSET_IDS_GET
		};

	private:
		AssetRequestType _requestType;
		boost::posix_time::ptime _submitted;
		
	public:
		AssetRequest(AssetRequestType type);
		AssetRequest(const AssetRequest& rhs);
		virtual ~AssetRequest();

		AssetRequest& operator =(const AssetRequest& rhs);
		
		/**
		 * Returns the type of request this is
		 */
		AssetRequestType getType() const;

		/**
		 * Processes this request
		 */
		virtual void processRequest(VFSBackend& be) = 0;

		/**
		 * Returns a description of the request
		 */
		virtual std::string getDescription() = 0;

		/**
		 * Returns how long ago this request was submitted in ms
		 */
		int queueDuration() const;
	};



	/**
	 * A request to retrieve an asset
	 */
	class AssetGetRequest : public AssetRequest
	{
	private:
		std::string _assetId;
		AsyncGetAssetCallback _callback;
		
	public:
		AssetGetRequest(const std::string& assetId, AsyncGetAssetCallback callback);
		AssetGetRequest(const AssetGetRequest& rhs);
		virtual ~AssetGetRequest();

		AssetGetRequest& operator =(const AssetGetRequest& rhs);

		virtual void processRequest(VFSBackend& be);

		virtual std::string getDescription();
	};



	/**
	 * A request to store an asset
	 */
	class AssetPutRequest : public AssetRequest
	{
	private:
		Asset::ptr _asset;
		AsyncStoreAssetCallback _callback;
		
	public:
		AssetPutRequest(Asset::ptr asset, AsyncStoreAssetCallback callback);
		AssetPutRequest(const AssetPutRequest& rhs);
		virtual ~AssetPutRequest();

		AssetPutRequest& operator =(const AssetPutRequest& rhs);

		virtual void processRequest(VFSBackend& be);

		virtual std::string getDescription();
	};



	/**
	 * A request to purge an asset
	 */
	class AssetPurgeRequest : public AssetRequest
	{
	private:
		std::string _assetId;
		AsyncAssetPurgeCallback _callback;
		bool _isPurgeLocalsRequest;
		
	public:
		AssetPurgeRequest(const std::string& assetId, bool isPurgeLocalsRequest, AsyncAssetPurgeCallback callback);
		AssetPurgeRequest(const std::string& assetId, bool isPurgeLocalsRequest);
		AssetPurgeRequest(const AssetPurgeRequest& rhs);
		virtual ~AssetPurgeRequest();

		AssetPurgeRequest& operator =(const AssetPurgeRequest& rhs);

		virtual void processRequest(VFSBackend& be);

		virtual std::string getDescription();
	};



	/**
	 * A request to physically delete local asset storage files
	 */
	class DeleteLocalStorageRequest : public AssetRequest
	{
	private:
		std::string _uuidPrefix;
		
	public:
		DeleteLocalStorageRequest(const std::string& uuidPrefix);
		DeleteLocalStorageRequest(const DeleteLocalStorageRequest& rhs);
		virtual ~DeleteLocalStorageRequest();

		DeleteLocalStorageRequest& operator =(const DeleteLocalStorageRequest& rhs);

		virtual void processRequest(VFSBackend& be);

		virtual std::string getDescription();
	};



	/**
	 * A request to return the status of the VFS storage system
	 */
	class CollectStatusRequest : public AssetRequest
	{
	private:
		boost::shared_ptr<std::ostream> _journal;
		boost::function<void()> _callback;
		
	public:
		CollectStatusRequest(boost::shared_ptr<std::ostream> journal, boost::function<void()> callback);
		CollectStatusRequest(const CollectStatusRequest& rhs);
		virtual ~CollectStatusRequest();

		CollectStatusRequest& operator =(const CollectStatusRequest& rhs);

		virtual void processRequest(VFSBackend& be);

		virtual std::string getDescription();
	};


	/**
	 * A request to return the status of the VFS storage system
	 */
	class GetStoredAssetIdsRequest : public AssetRequest
	{
	private:
		std::string _prefix;
		boost::shared_ptr<std::ostream> _journal;
		boost::function<void()> _callback;
		
	public:
		GetStoredAssetIdsRequest(std::string prefix, boost::shared_ptr<std::ostream> journal, boost::function<void()> callback);
		GetStoredAssetIdsRequest(const GetStoredAssetIdsRequest& rhs);
		virtual ~GetStoredAssetIdsRequest();

		GetStoredAssetIdsRequest& operator =(const GetStoredAssetIdsRequest& rhs);

		virtual void processRequest(VFSBackend& be);

		virtual std::string getDescription();
	};
}