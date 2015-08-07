#pragma once

#include <boost/shared_ptr.hpp>
#include "byte.h"

/**
 * Enumeration for the auth staus codes
 */
enum AuthStatus
{
	AS_AUTH_SUCCESS = 0,
	AS_AUTH_FAILURE = 1
};

/**
 * The server's response to the client's auth response.  Indicates if the client is
 * allowed to continue processing
 */
class AuthStatusMsg
{
private:
	static const whip::byte PACKET_IDENTIFIER;

	whip::byte_array _messageData;

public:
	static const short MESSAGE_SIZE;
	typedef boost::shared_ptr<AuthStatusMsg> ptr;

public:
	/**
	Construct a new auth status message
	*/
	AuthStatusMsg();
	AuthStatusMsg(AuthStatus authStatus);
	virtual ~AuthStatusMsg();

	bool validate() const;

	const whip::byte_array& serialize() const;
	whip::byte_array& getDataStorage();

	AuthStatus getAuthStatus() const;
};
