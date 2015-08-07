#pragma once

#include <boost/shared_ptr.hpp>

#include "byte.h"
#include "Asset.h"

/**
Request from a client for an action on an asset
*/
class ClientRequestMsg
{
private:
	whip::byte_array _header;
	whip::byte_array_ptr _data;

public:
	typedef boost::shared_ptr<ClientRequestMsg> ptr;

	enum RequestType
	{
		RT_GET = 10,
		RT_PUT = 11,
		RT_PURGE = 12,
		RT_TEST = 13,
		RT_MAINT_PURGELOCALS = 14,
		RT_STATUS_GET = 15,
		RT_STORED_ASSET_IDS_GET = 16,
		RT_GET_DONTCACHE = 17
	};

	static const short HEADER_SIZE = 37;
	static const short DATA_SIZE_MARKER_LOC = 33;

public:
	ClientRequestMsg();

	/**
	 * CTOR for a get request
	 */
	ClientRequestMsg(RequestType type, const std::string& uuid);

	/**
	 * CTOR for a put request
	 */
	explicit ClientRequestMsg(Asset::ptr asset);


	virtual ~ClientRequestMsg();
	
	/**
	Returns the raw storage for the header
	*/
	whip::byte_array& getHeaderData();

	/**
	Returns the type of message this is
	*/
	RequestType getType() const;

	/**
	Returns the UUID this message applies to
	*/
	std::string getUUID() const;

	/**
	Returns the data from this request
	*/
	whip::byte_array_ptr getData();

	/**
	Initializes data array given the size in the header
	*/
	void initDataStorageFromHeader();

	/**
	Returns the size of the data for this request
	*/
	unsigned int getDataSize() const;
};
