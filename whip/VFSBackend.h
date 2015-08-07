#pragma once

#include "IStorageBackend.h"
#include "IAsyncStorageBackend.h"
#include "DatabaseSet.h"
#include "ExistenceIndex.h"
#include "AsyncStorageTypes.h"
#include "VFSQueueTypes.h"
#include "MovingAverage.h"
#include "PushReplicationService.h"

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <iosfwd>
#include <deque>
#include <string>

/**
 *	Namespace for InWorldz virtual file system components
 */
namespace iwvfs
{
	/**
		InWorldz virtual file system storage backend.  Provides access
		to an indexed file system that uses the first 3 letters of the hex
		asset id to set up 4096 folders that each contain a database and index
		file for local and global assets
	*/
	class VFSBackend : public IStorageBackend, public IAsyncStorageBackend
	{
	private:
		const static int DISK_LATENCY_SAMPLE_SIZE = 50;

		std::string _storageRoot;
		bool _enablePurge;

		bool _debugging;
		bool _stop;

		ExistenceIndex::ptr _existenceIndex;
	
		boost::shared_ptr<boost::thread> _workerThread;
		boost::mutex _workMutex;
		boost::condition_variable _workArrived;

		std::deque<AssetRequest::ptr> _workQueue;

		boost::asio::io_service& _ioService;

		boost::asio::deadline_timer _purgeLocalsTimer;
		bool _isPurgingLocals;
		int _currentLocalsPurgeIndex;

		MovingAverage _diskLatencyAvg;
		MovingAverage _diskOpAvg;

		PushReplicationService::ptr _pushRepl;


		bool isLittleEndian();
		boost::filesystem::path getFullPathForUUID(const std::string& uuid);
		void verifyDatabaseDirectory(const std::string& uuid);
		DatabaseSet::ptr verifyAndopenSetForUUID(const std::string& uuid);
		void workLoop();

		void onPurgeTimer(const boost::system::error_code& error);
		
		void unindexLocals(UuidListPtr uuidList, std::string uuidHead);

		void assetWriteFailed(std::string assetId, std::string reason);
		void ioLoopFailed(std::string reason);

	protected:
		void performDiskWrite(Asset::ptr asset, AsyncStoreAssetCallback callback);
		void performDiskRead(const std::string& assetId, AsyncGetAssetCallback callback);
		void performDiskPurge(const std::string& assetId, AsyncAssetPurgeCallback callback);
		void performDiskPurgeLocals(const std::string& assetId);
		void performLocalsPhysicalDeletion(const std::string& uuidHead);
		void performGetStatus(boost::shared_ptr<std::ostream> journal, boost::function<void()> completedCallback);
		void performGetStoredAssetIds(std::string prefix, boost::shared_ptr<std::ostream> journal, boost::function<void()> completedCallback);

	public:
		friend class AssetGetRequest;
		friend class AssetPutRequest;
		friend class AssetPurgeRequest;
		friend class DeleteLocalStorageRequest;
		friend class CollectStatusRequest;
		friend class GetStoredAssetIdsRequest;

	public:
		VFSBackend(const std::string& storageRoot, bool enablePurge, boost::asio::io_service& ioService,
			PushReplicationService::ptr pushRepl);
		virtual ~VFSBackend();

		virtual Asset::ptr getAsset(const std::string& uuid);
		virtual void storeAsset(Asset::ptr asset);
		virtual bool assetExists(const std::string& uuid);
		virtual void purgeAsset(const std::string& uuid);

		virtual void getAsset(const std::string& uuid, AsyncGetAssetCallback callBack);
		virtual void getAsset(const std::string& uuid, unsigned int flags, AsyncGetAssetCallback callBack);
		virtual void storeAsset(Asset::ptr asset, AsyncStoreAssetCallback callBack);
		virtual void purgeAsset(const std::string& uuid, AsyncAssetPurgeCallback callBack);

		virtual void beginPurgeLocals();

		virtual void getStatus(boost::shared_ptr<std::ostream> journal, boost::function<void()> completedCallback);
		virtual void getStoredAssetIDsWithPrefix(std::string prefix, boost::shared_ptr<std::ostream> journal, boost::function<void()> completedCallback);

		virtual void shutdown();

		ExistenceIndex::ptr getIndex();
	};

}