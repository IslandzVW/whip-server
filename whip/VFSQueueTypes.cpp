#include "StdAfx.h"

#include "VFSQueueTypes.h"
#include "VFSBackend.h"

using namespace boost::posix_time;

namespace iwvfs
{
	AssetRequest::AssetRequest(AssetRequestType type)
		: _requestType(type), _submitted(microsec_clock::universal_time())
	{

	}

	AssetRequest::AssetRequest(const AssetRequest& rhs)
		: _requestType(rhs._requestType), _submitted(rhs._submitted)
	{
		
	}

	AssetRequest& AssetRequest::operator =(const AssetRequest& rhs)
	{
		if (this != &rhs) {
			_requestType = rhs._requestType;
			_submitted = rhs._submitted;
		}

		return *this;
	}
	
	AssetRequest::AssetRequestType AssetRequest::getType() const
	{
		return _requestType;
	}

	int AssetRequest::queueDuration() const
	{
		time_duration duration = microsec_clock::universal_time() - _submitted;
		return (int)duration.total_milliseconds();
	}

	AssetRequest::~AssetRequest()
	{
	}



	AssetGetRequest::AssetGetRequest(const std::string& assetId, AsyncGetAssetCallback callback)
		: AssetRequest(AssetRequest::AR_GET), _assetId(assetId), _callback(callback)
	{
	}

	AssetGetRequest::AssetGetRequest(const AssetGetRequest& rhs)
		: AssetRequest(rhs), _assetId(rhs._assetId), _callback(rhs._callback)
	{

	}

	AssetGetRequest::~AssetGetRequest()
	{
	}

	AssetGetRequest& AssetGetRequest::operator =(const AssetGetRequest& rhs)
	{
		if (this != &rhs) {
			AssetRequest::operator=(rhs);
			_assetId = rhs._assetId;
			_callback = rhs._callback;
		}

		return *this;
	}

	void AssetGetRequest::processRequest(VFSBackend& be)
	{
		be.performDiskRead(_assetId, _callback);
	}

	std::string AssetGetRequest::getDescription()
	{
		return "GET " + _assetId;
	}




	AssetPutRequest::AssetPutRequest(Asset::ptr asset, AsyncStoreAssetCallback callback)
		: AssetRequest(AssetRequest::AR_PUT), _asset(asset), _callback(callback)
	{
	}

	AssetPutRequest::AssetPutRequest(const AssetPutRequest& rhs)
		: AssetRequest(rhs), _asset(rhs._asset), _callback(rhs._callback)
	{

	}

	AssetPutRequest::~AssetPutRequest()
	{
	}

	AssetPutRequest& AssetPutRequest::operator =(const AssetPutRequest& rhs)
	{
		if (this != &rhs) {
			AssetRequest::operator=(rhs);
			_asset = rhs._asset;
			_callback = rhs._callback;
		}

		return *this;
	}

	void AssetPutRequest::processRequest(VFSBackend& be)
	{
		be.performDiskWrite(_asset, _callback);
	}

	std::string AssetPutRequest::getDescription()
	{
		return "PUT " + _asset->getUUID();
	}




	AssetPurgeRequest::AssetPurgeRequest(const std::string& assetId, bool isPurgeLocalsRequest, AsyncAssetPurgeCallback callback)
		: AssetRequest(isPurgeLocalsRequest ? AssetRequest::AR_PURGE_LOCALS : AssetRequest::AR_PURGE),
		_assetId(assetId), _callback(callback), _isPurgeLocalsRequest(isPurgeLocalsRequest)
	{
	}

	AssetPurgeRequest::AssetPurgeRequest(const std::string& assetId, bool isPurgeLocalsRequest)
		: AssetRequest(isPurgeLocalsRequest ? AssetRequest::AR_PURGE_LOCALS : AssetRequest::AR_PURGE),
		_assetId(assetId), _isPurgeLocalsRequest(isPurgeLocalsRequest)
	{
	}

	AssetPurgeRequest::AssetPurgeRequest(const AssetPurgeRequest& rhs)
		: AssetRequest(rhs), _assetId(rhs._assetId), _callback(rhs._callback), _isPurgeLocalsRequest(rhs._isPurgeLocalsRequest)
	{
	}

	AssetPurgeRequest::~AssetPurgeRequest()
	{
	}

	AssetPurgeRequest& AssetPurgeRequest::operator =(const AssetPurgeRequest& rhs)
	{
		if (this != &rhs) {
			AssetRequest::operator =(rhs);
			_assetId = rhs._assetId;
			_callback = rhs._callback;
			_isPurgeLocalsRequest = rhs._isPurgeLocalsRequest;
		}

		return *this;
	}

	void AssetPurgeRequest::processRequest(VFSBackend& be)
	{
		if (_isPurgeLocalsRequest) {
			be.performDiskPurgeLocals(_assetId);

		} else {
			be.performDiskPurge(_assetId, _callback);

		}
	}

	std::string AssetPurgeRequest::getDescription()
	{
		return "PURGE";
	}



	DeleteLocalStorageRequest::DeleteLocalStorageRequest(const std::string& uuidPrefix)
		: AssetRequest(AssetRequest::AR_DELETE_LOCAL_STORAGE), _uuidPrefix(uuidPrefix)
	{
	}

	DeleteLocalStorageRequest::DeleteLocalStorageRequest(const DeleteLocalStorageRequest& rhs)
		: AssetRequest(rhs), _uuidPrefix(rhs._uuidPrefix)
	{
	}

	DeleteLocalStorageRequest::~DeleteLocalStorageRequest()
	{
	}

	DeleteLocalStorageRequest& DeleteLocalStorageRequest::operator =(const DeleteLocalStorageRequest& rhs)
	{
		if (this != &rhs) {
			AssetRequest::operator =(rhs);
			_uuidPrefix = rhs._uuidPrefix;
		}

		return *this;
	}

	void DeleteLocalStorageRequest::processRequest(VFSBackend& be)
	{
		be.performLocalsPhysicalDeletion(_uuidPrefix);
	}

	std::string DeleteLocalStorageRequest::getDescription()
	{
		return "DELETE LOCAL STORAGE " + _uuidPrefix;
	}



	CollectStatusRequest::CollectStatusRequest(boost::shared_ptr<std::ostream> journal, boost::function<void()> callback)
		: AssetRequest(AssetRequest::AR_STATUS_GET), _journal(journal), _callback(callback)
	{

	}

	CollectStatusRequest::CollectStatusRequest(const CollectStatusRequest& rhs)
		: AssetRequest(rhs), _journal(rhs._journal), _callback(rhs._callback)
	{
	}

	CollectStatusRequest::~CollectStatusRequest()
	{
	}

	CollectStatusRequest& CollectStatusRequest::operator =(const CollectStatusRequest& rhs)
	{
		if (&rhs != this) {
			_journal = rhs._journal;
			_callback = rhs._callback;
		}

		return *this;
	}

	void CollectStatusRequest::processRequest(VFSBackend& be)
	{
		be.performGetStatus(_journal, _callback);
	}

	std::string CollectStatusRequest::getDescription()
	{
		return "COLLECT STATUS";
	}



	GetStoredAssetIdsRequest::GetStoredAssetIdsRequest(std::string prefix, boost::shared_ptr<std::ostream> journal, boost::function<void()> callback)
		: AssetRequest(AssetRequest::AR_STORED_ASSET_IDS_GET), _prefix(prefix), _journal(journal), _callback(callback)
	{

	}

	GetStoredAssetIdsRequest::GetStoredAssetIdsRequest(const GetStoredAssetIdsRequest& rhs)
		: AssetRequest(rhs), _prefix(rhs._prefix), _journal(rhs._journal), _callback(rhs._callback)
	{
	}

	GetStoredAssetIdsRequest::~GetStoredAssetIdsRequest()
	{
	}

	GetStoredAssetIdsRequest& GetStoredAssetIdsRequest::operator =(const GetStoredAssetIdsRequest& rhs)
	{
		if (&rhs != this) {
			_prefix = rhs._prefix;
			_journal = rhs._journal;
			_callback = rhs._callback;
		}

		return *this;
	}

	void GetStoredAssetIdsRequest::processRequest(VFSBackend& be)
	{
		be.performGetStoredAssetIds(_prefix, _journal, _callback);
	}

	std::string GetStoredAssetIdsRequest::getDescription()
	{
		return "GET STORED IDS";
	}
}