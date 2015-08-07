#pragma once

#include <boost/shared_ptr.hpp>

#include "byte.h"
#include "AuthChallengeMsg.h"

/**
 * The client's response to an auth challenge
 */
class AuthResponseMsg
{
private:
	const static whip::byte PACKET_IDENTIFIER;
	const static whip::byte SERVER_IDENTIFIER;

	const static short RESPONSE_SIZE;
	
	whip::byte_array _data;

public:
	const static short MESSAGE_SIZE;
	typedef boost::shared_ptr<AuthResponseMsg> ptr;

public:
	AuthResponseMsg();
	AuthResponseMsg(const std::string& challenge, const std::string& password);
	virtual ~AuthResponseMsg();

	/**
	 * Returns the internal storage array for use in an asio buffer
	 */
	whip::byte_array& getDataStorage();

	/**
	 * Returns just the challenge response portion of this message
	 */
	std::string getChallengeResponse() const;

	/**
	 * Tests if the response is valid
	 */
	bool isValid(AuthChallengeMsg::ptr authChallenge);

	/**
	 * Calculates the correct challenge response from the given challenge phrase
	 * and password
	 */
	std::string calculateChallengeResponse(const std::string& phrase, const std::string& password);

	/**
	 * Determines whether or not the responder is a server
	 */
	bool isServerResponse() const;
};
