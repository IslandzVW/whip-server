#pragma once

#include "byte.h"
#include <boost/shared_ptr.hpp>

/**
 * The server's response to a cllient request
 */
class ServerResponseMsg
{
private:
	whip::byte_array_ptr _data;
	whip::byte_array _header;

	static const short DATA_LEN_MARKER_LOC = 33;
	static const short RESPONSE_CODE_MARKER_LOC = 0;

public:
	typedef boost::shared_ptr<ServerResponseMsg> ptr;

	static const short HEADER_SIZE = 37;
	
	enum ResponseCode {
		RC_FOUND = 10,
		RC_NOTFOUND = 11,
		RC_ERROR = 12,
		RC_OK = 13
	};

private:
	void setupHeader(ResponseCode code, const std::string& assetUUID, unsigned int dataLen);

public:
	/**
	 * Constructs a new response message for pooling
	 */
	ServerResponseMsg();

	/**
	 *Constructs a new response message with the given code and no data
	 */
	ServerResponseMsg(ResponseCode code, const std::string& assetUUID);

	/**
	 * Constructs a new response message with the given code and data
	 */
	ServerResponseMsg(ResponseCode code, const std::string& assetUUID, whip::byte_array_ptr data);

	/**
	 *Constructs a new response message with the given code and string for data
	 */
	ServerResponseMsg(ResponseCode code, const std::string& assetUUID, const std::string& message);


	/**
	DTOR
	*/
	virtual ~ServerResponseMsg();


	/**
	Returns the data array that holds the header
	*/
	whip::byte_array& getHeader();

	/**
	Returns the array that holds the data
	*/
	whip::byte_array_ptr getData();

	/**
	 * Allocates the memory for inbound asset data based on the data in the header
	 */
	void initializeDataStorage();

	/**
	 * Validates the header for this message
	 */
	bool validateHeader();

	/**
	 * Returns the response code
	 */
	ResponseCode getResponseCode() const;

	/**
	 * Returns the size of the incoming data according to this header
	 */
	unsigned int getDataSize() const;

	/**
	 * Returns the UUID of the asset that generated the response
	 */
	std::string getUUID() const;
};

class ServerResponseMsgCreator
{
public: 

	ServerResponseMsg::ptr operator()()
	{
		return ServerResponseMsg::ptr(new ServerResponseMsg());
	}
};