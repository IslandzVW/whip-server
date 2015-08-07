#include "StdAfx.h"
#include "VFSBackend.h"

#include "AppLog.h"
#include "AssetStorageError.h"
#include "Settings.h"
#include "DatabaseSet.h" 
#include "IndexFile.h"
#include "SQLiteIndexFileManager.h"
#include "IIndexFileManager.h"
#include "kashmir/uuid.h"
#include "SQLiteError.h"

#include <boost/filesystem/fstream.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/shared_array.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/foreach.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <stdexcept>
#include <sstream>
#include <vector>

#include <boost/cstdlib.hpp>

namespace fs = boost::filesystem;
using namespace boost::posix_time;

namespace iwvfs
{

	VFSBackend::VFSBackend(const std::string& storageRoot, bool enablePurge, boost::asio::io_service& ioService,
		PushReplicationService::ptr pushRepl)
		: _storageRoot(storageRoot), _enablePurge(enablePurge), _stop(false), _ioService(ioService), _purgeLocalsTimer(ioService),
		_isPurgingLocals(false), _currentLocalsPurgeIndex(0), _diskLatencyAvg(DISK_LATENCY_SAMPLE_SIZE),
		_diskOpAvg(DISK_LATENCY_SAMPLE_SIZE), _pushRepl(pushRepl)
	{
		SAFELOG(AppLog::instance().out() << "[IWVFS] Starting storage backend" << std::endl);

		//make sure the storage root exists
		if (! fs::exists(_storageRoot)) {
			throw std::runtime_error("Unable to start storage backend, the storage root '" 
				+ storageRoot + "' was not found");
		}

		SAFELOG(AppLog::instance().out() << "[IWVFS] Root: " << storageRoot << ", Purge: " << (_enablePurge ? "enabled" : "disabled") << std::endl);
		_debugging = whip::Settings::instance().get("debug").as<bool>();

		SAFELOG(AppLog::instance().out() << "[IWVFS] SQLite Index Backend: SQLite v" << sqlite3_libversion() << std::endl);

		//TODO:  Should be deleted on shutdown
		IIndexFileManager::ptr indexFileManager(new SQLiteIndexFileManager());
		IndexFile::SetIndexFileManager(indexFileManager);

		SAFELOG(AppLog::instance().out() << "[IWVFS] Generating asset existence index" << std::endl);
		_existenceIndex.reset(new ExistenceIndex(storageRoot));

		SAFELOG(AppLog::instance().out() << "[IWVFS] Starting disk I/O worker thread" << std::endl);
		_workerThread.reset(new boost::thread(boost::bind(&VFSBackend::workLoop, this)));
	}

	VFSBackend::~VFSBackend()
	{
	}

	void VFSBackend::workLoop()
	{
		while( !_stop )
        {
			try {
				AssetRequest::ptr req;

				{
					boost::mutex::scoped_lock lock(_workMutex);
					while(_workQueue.empty() && !_stop) {
						_workArrived.wait(lock);
					}

					if (_stop) break;

					req = _workQueue.front();
					_workQueue.pop_front();
				}

				_diskLatencyAvg.addSample(req->queueDuration());

				ptime opStart = microsec_clock::universal_time();
				req->processRequest(*this);
				time_duration diff = microsec_clock::universal_time() - opStart;
				_diskOpAvg.addSample((int) diff.total_milliseconds());

			} catch (const std::exception& e) {
				_ioService.post(boost::bind(&VFSBackend::ioLoopFailed, this, std::string(e.what())));

			} catch (...) {
				_ioService.post(boost::bind(&VFSBackend::ioLoopFailed, this, "Unknown error"));
			}
        }
	}

	void VFSBackend::performDiskWrite(Asset::ptr asset, AsyncStoreAssetCallback callback)
	{
		try {
			this->storeAsset(asset);
			_ioService.post(boost::bind(callback, true, AssetStorageError::ptr()));

			_pushRepl->queueAssetForPush(asset);

		} catch (const AssetStorageError& storageError) {
			AssetStorageError::ptr error(new AssetStorageError(storageError));
			_ioService.post(boost::bind(callback, false, error));

		} catch (const std::exception& storageError) {
			AssetStorageError::ptr error(new AssetStorageError(storageError.what()));
			_ioService.post(boost::bind(&VFSBackend::assetWriteFailed, this, asset->getUUID(), storageError.what()));
			_ioService.post(boost::bind(callback, false, error));
		}
	}

	void VFSBackend::assetWriteFailed(std::string assetId, std::string reason)
	{
		SAFELOG(AppLog::instance().error() << "[IWVFS] Asset write failed for " << assetId << ": " << reason << std::endl);
		_existenceIndex->removeId(kashmir::uuid::uuid_t(assetId.c_str()));
	}

	void VFSBackend::ioLoopFailed(std::string reason)
	{
		SAFELOG(AppLog::instance().error() << "[IWVFS] Critical warning: ioWorker caught exception: " << reason << std::endl);
	}

	void VFSBackend::performDiskRead(const std::string& assetId, AsyncGetAssetCallback callback)
	{
		try {
			Asset::ptr asset = this->getAsset(assetId);

			if (asset) {
				_ioService.post(boost::bind(callback, asset, true, AssetStorageError::ptr()));

			} else {
				AssetStorageError::ptr error(new AssetStorageError("Asset " + assetId + " not found"));
				_ioService.post(boost::bind(callback, Asset::ptr(), false, error));

			}

		} catch (const AssetStorageError& storageError) {
			AssetStorageError::ptr error(new AssetStorageError(storageError));
			_ioService.post(boost::bind(callback, Asset::ptr(), false, error));

		} catch (const std::exception& storageError) {
			AssetStorageError::ptr error(new AssetStorageError(storageError.what()));
			_ioService.post(boost::bind(callback, Asset::ptr(), false, error));
		}
	}

	void VFSBackend::performDiskPurge(const std::string& assetId, AsyncAssetPurgeCallback callback)
	{

	}

	void VFSBackend::performDiskPurgeLocals(const std::string& uuidHead)
	{
		//purging locals has three steps which starts here
		// 1) The first step is to look up the asset ids that are contained in the specified index/locals file
		// 2) Those local ids are passed to the existence index for deletion
		// 3) When that process completes a full deletion is scheduled for the locals data file and index

		fs::path indexPath(fs::path(_storageRoot) / uuidHead / "locals.idx");

		UuidListPtr containedIds;
		if (fs::exists(indexPath)) {		
			//open the index file and get a list of asset ids contained within
			IndexFile::ptr indexFile(new IndexFile(indexPath));
			containedIds = indexFile->getContainedAssetIds();
		}

		//schedule a deletion of the returned uuids to happen on the ASIO thread
		_ioService.post(boost::bind(&VFSBackend::unindexLocals, this, containedIds, uuidHead));
	}

	void VFSBackend::unindexLocals(UuidListPtr uuidList, std::string uuidHead)
	{
		if (uuidList) {
			UuidList::iterator end = uuidList->end();
			for (UuidList::iterator i = uuidList->begin(); i != end; ++i)
			{
				_existenceIndex->removeId(*i);
			}
		}

		//schedule physical deletion of the index and data files
		AssetRequest::ptr req(new DeleteLocalStorageRequest(uuidHead));

		boost::mutex::scoped_lock lock(_workMutex);
		_workQueue.push_back(req);
		_workArrived.notify_one();
	}

	void VFSBackend::performLocalsPhysicalDeletion(const std::string& uuidHead)
	{
		fs::path indexPath(fs::path(_storageRoot) / uuidHead / "locals.idx");

		IndexFile::GetIndexFileManager()->forceCloseIndexFile(indexPath);

		//the index file is guaranteed to not be in use, delete the index file and the data file
		fs::path dataPath(fs::path(_storageRoot) / uuidHead / "locals.data");

		fs::remove(indexPath);
		fs::remove(dataPath);
	}

	bool VFSBackend::isLittleEndian()
	{
		union {
			boost::uint32_t i;
			char c[4];
		} bint = {0x01020304};

		return bint.c[0] != 1; 
	}

	fs::path VFSBackend::getFullPathForUUID(const std::string& uuid)
	{
		fs::path subdir(fs::path(_storageRoot) / fs::path(uuid.substr(0, whip::Settings::UUID_PATH_CHARS)));

		return subdir;
	}

	void VFSBackend::verifyDatabaseDirectory(const std::string& uuid)
	{
		fs::path subdir(this->getFullPathForUUID(uuid));

		if (! fs::exists(subdir)) {
			if (! fs::create_directory(subdir)) {
				throw AssetStorageError("Could not create directory for database storage " + subdir.string(), true);
			}
		}
	}

	DatabaseSet::ptr VFSBackend::verifyAndopenSetForUUID(const std::string& uuid)
	{
		try {
			this->verifyDatabaseDirectory(uuid);

			DatabaseSet::ptr dbset(new DatabaseSet(this->getFullPathForUUID(uuid)));
			return dbset;

		} catch (const SQLiteError& e) {
			throw AssetStorageError(std::string("Index file error: ") + e.what(), true);
		}

	}

	Asset::ptr VFSBackend::getAsset(const std::string& uuid)
	{
		DatabaseSet::ptr dbset(this->verifyAndopenSetForUUID(uuid));
		
		Asset::ptr localAsset(dbset->getAsset(uuid));
		return localAsset;
	}

	void VFSBackend::storeAsset(Asset::ptr asset)
	{
		DatabaseSet::ptr dbset(this->verifyAndopenSetForUUID(asset->getUUID()));
		
		dbset->storeAsset(asset);
	}

	bool VFSBackend::assetExists(const std::string& uuid)
	{
		return _existenceIndex->assetExists(uuid);
	}

	void VFSBackend::purgeAsset(const std::string& uuid)
	{
		//TODO: Not implemented
	}
	
	void VFSBackend::getAsset(const std::string& uuid, unsigned int flags, AsyncGetAssetCallback callBack)
	{
		//flags are not used here yet
		this->getAsset(uuid, callBack);
	}

	void VFSBackend::getAsset(const std::string& uuid, AsyncGetAssetCallback callBack)
	{
		//use the cache to shortcut the situation where the asset doesnt exist
		if (! _existenceIndex->assetExists(uuid)) {
			AssetStorageError::ptr error(new AssetStorageError("Could not retrieve asset " + uuid + ", asset not found"));
			_ioService.post(boost::bind(callBack, Asset::ptr(), false, error));
			return;
		}

		AssetRequest::ptr req(new AssetGetRequest(uuid, callBack));

		boost::mutex::scoped_lock lock(_workMutex);
		_workQueue.push_back(req);
		_workArrived.notify_one();
	}

	void VFSBackend::storeAsset(Asset::ptr asset, AsyncStoreAssetCallback callBack)
	{
		//use the cache to shortcut the situation where the asset already exists
		if (_existenceIndex->assetExists(asset->getUUID())) {
			AssetStorageError::ptr error(new AssetStorageError("Unable to store asset " + asset->getUUID() + ", asset already exists"));
			_ioService.post(boost::bind(callBack, false, error));
			return;
		}

		//we put the asset into the existance index now to be sure there are no race conditions
		//between storage and subsequent retrieval 
		//	remember the queue will process all
		//	requests in order so getting a successful response for a read here is ok since
		//	the write will always happen first
		_existenceIndex->addNewId(asset->getUUID());


		AssetRequest::ptr req(new AssetPutRequest(asset, callBack));

		boost::mutex::scoped_lock lock(_workMutex);
		_workQueue.push_back(req);
		_workArrived.notify_one();
	}

	void VFSBackend::purgeAsset(const std::string& uuid, AsyncAssetPurgeCallback callBack)
	{
		AssetRequest::ptr req(new AssetPurgeRequest(uuid, false, callBack));

		boost::mutex::scoped_lock lock(_workMutex);
		_workQueue.push_back(req);
		_workArrived.notify_one();
	}

	void VFSBackend::beginPurgeLocals()
	{
		if (! _isPurgingLocals) {
			SAFELOG(AppLog::instance().out() << "[IWVFS] PurgeLocals command received, beginning purge" << std::endl);
			_isPurgingLocals = true;
			_currentLocalsPurgeIndex = 0;
			_purgeLocalsTimer.expires_from_now(boost::posix_time::seconds(1));
			_purgeLocalsTimer.async_wait(boost::bind(&VFSBackend::onPurgeTimer, this, boost::asio::placeholders::error));
		}

	}

	void VFSBackend::onPurgeTimer(const boost::system::error_code& error)
	{
		if (!error) {
			std::stringstream hexNum;
			hexNum << std::setfill('0') << std::setw(whip::Settings::UUID_PATH_CHARS) << std::hex << _currentLocalsPurgeIndex++;

			std::string assetId(hexNum.str());
			AssetRequest::ptr req(new AssetPurgeRequest(assetId, true));

			SAFELOG(AppLog::instance().out() << "[IWVFS] Queueing purge of: " << assetId << std::endl);

			boost::mutex::scoped_lock lock(_workMutex);
			_workQueue.push_back(req);
			_workArrived.notify_one();

			if (_currentLocalsPurgeIndex <= 0xFFF) {
				_purgeLocalsTimer.expires_from_now(boost::posix_time::seconds(1));
				_purgeLocalsTimer.async_wait(boost::bind(&VFSBackend::onPurgeTimer, this, boost::asio::placeholders::error));
			} else {
				_isPurgingLocals = false;
			}

		} else {
			SAFELOG(AppLog::instance().out() << "[IWVFS] PurgeLocals timer execution error: " << error.message() << std::endl);
		}
	}

	void VFSBackend::getStatus(boost::shared_ptr<std::ostream> journal, boost::function<void()> completedCallback)
	{
		AssetRequest::ptr req(new CollectStatusRequest(journal, completedCallback));

		boost::mutex::scoped_lock lock(_workMutex);
		_workQueue.push_back(req);
		_workArrived.notify_one();
	}

	void VFSBackend::performGetStatus(boost::shared_ptr<std::ostream> journal, boost::function<void()> completedCallback)
	{
		(*journal) << "-VFS Backend" << std::endl;

		{
			boost::mutex::scoped_lock lock(_workMutex);

			(*journal) << "  Disk queue size: " << _workQueue.size() << std::endl;
			(*journal) << "  Avg Disk Queue Wait: " << _diskLatencyAvg.getAverage() << " ms" << std::endl;
			(*journal) << "  Avg Disk Op Latency: " << _diskOpAvg.getAverage() << " ms" << std::endl;
			(*journal) << "-VFS Queue Items" << std::endl;

			BOOST_FOREACH(AssetRequest::ptr req, _workQueue)
			{
				(*journal) << "  " << req->getDescription() << std::endl;
			}
		}

		_ioService.post(completedCallback);
	}

	void VFSBackend::getStoredAssetIDsWithPrefix(std::string prefix, boost::shared_ptr<std::ostream> journal, boost::function<void()> completedCallback)
	{
		AssetRequest::ptr req(new GetStoredAssetIdsRequest(prefix, journal, completedCallback));

		boost::mutex::scoped_lock lock(_workMutex);
		_workQueue.push_back(req);
		_workArrived.notify_one();
	}

	void VFSBackend::performGetStoredAssetIds(std::string prefix, boost::shared_ptr<std::ostream> journal, boost::function<void()> completedCallback)
	{
		fs::path indexPath(fs::path(_storageRoot) / prefix / "globals.idx");
		
		if (fs::exists(indexPath)) {
			//open the index file and get a list of asset ids contained within
			IndexFile::ptr indexFile(new IndexFile(indexPath));
			StringUuidListPtr containedIds = indexFile->getContainedAssetIdStrings();
			StringUuidList& containedIdsRef = *containedIds;

			BOOST_FOREACH(const std::string& assetId, containedIdsRef)
			{
				(*journal) << assetId << ",";
			}
		}

		_ioService.post(completedCallback);
	}

	void VFSBackend::shutdown()
	{
		SAFELOG(AppLog::instance().out() << "[IWVFS] Performing clean shutdown" << std::endl);

		_stop = true;

		{
			boost::mutex::scoped_lock lock(_workMutex);
			_workArrived.notify_one();
		}

		if (_workerThread->joinable()) {
			_workerThread->join();
		}

		SAFELOG(AppLog::instance().out() << "[IWVFS] Closing indexes" << std::endl);
		IndexFile::GetIndexFileManager()->shutdown();

		SAFELOG(AppLog::instance().out() << "[IWVFS] Shutdown complete" << std::endl);
	}

	ExistenceIndex::ptr VFSBackend::getIndex()
	{
		return _existenceIndex;
	}
}