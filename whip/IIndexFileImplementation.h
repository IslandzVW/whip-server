#pragma once

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>	
#include <boost/cstdint.hpp>

#include "kashmir/uuid.h"

typedef std::vector<kashmir::uuid::uuid_t> UuidList;
typedef boost::shared_ptr<UuidList> UuidListPtr;

typedef std::vector<std::string> StringUuidList;
typedef boost::shared_ptr<StringUuidList> StringUuidListPtr;

/**
	Implementation of an index file that can locate an asset position in the data file
*/
class IIndexFileImplementation
{
public:
	typedef boost::shared_ptr<IIndexFileImplementation> ptr;

public:
	virtual ~IIndexFileImplementation();

	/**
	 * Returns the position of the given asset by it's UUID or INVALID_POSITION
	 */
	virtual boost::int64_t findAssetPosition(const std::string& uuid) = 0;

	/**
	 * Records the position of the given asset in the index file
	 */
	virtual void recordAssetPosition(Asset::ptr asset, boost::int64_t position) = 0;

	/**
	 * Returns the uuid of all assets tracked by this index
	 */
	virtual UuidListPtr getContainedAssetIds() = 0;

	/**
	 * Returns the uuid of all assets tracked by this index
	 */
	virtual StringUuidListPtr getContainedAssetIdStrings() = 0;
};
