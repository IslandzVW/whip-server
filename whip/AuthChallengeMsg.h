#pragma once
#include <boost/shared_ptr.hpp>

#include "byte.h"

/**
 * Message challenging a connector to the whip services
 */
class AuthChallengeMsg
{
private:
	const static short PHRASE_SIZE;
	const static whip::byte PACKET_IDENTIFIER;
	whip::byte_array _data;
	
	void generatePhrase();

public:
	const static short MESSAGE_SIZE = 8;
	typedef boost::shared_ptr<AuthChallengeMsg> ptr;

public:
	AuthChallengeMsg(bool inbound);
	virtual ~AuthChallengeMsg();

	/**
	 * Returns the byte array that represents this message on the network
	 */
	const whip::byte_array& serialize() const;
	
	/**
	 * Returns the byte array that represents this message on the network for writing
	 */
	whip::byte_array& getDataStorage();
	

	/**
	 * Returns the challenge phrase that was generated
	 */
	std::string getPhrase() const;
};
