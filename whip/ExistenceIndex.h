#pragma once

#include "kashmir/uuid.h"
#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

#include <set>


namespace iwvfs
{
/**
	Index loaded at server startup of all asset UUIDs contained on this server. 
	This removes the disk impact of many asset servers requesting assets
	on intramesh
*/
class ExistenceIndex
{
private:
	std::set<kashmir::uuid::uuid_t> _existingAssets;
	boost::mutex _lock;

	void processDirectory(boost::filesystem::path dir);
	void loadIndexFile(const boost::filesystem::path& path);
	void rowCallback(sqlite3_stmt* stmt);

public:
	typedef boost::shared_ptr<ExistenceIndex> ptr;

public:
	ExistenceIndex(const std::string& storageRoot);
	virtual ~ExistenceIndex();

	/**
		Returns true if this cache knows about the given asset
		false if not
	*/
	bool assetExists(const std::string& assetId);
	
	/**
		Tracks a new ID to this server
	*/
	void addNewId(const std::string& assetId);

	/**
	 * Removes an id from the index
	 */
	void removeId(const kashmir::uuid::uuid_t& id);
};

}