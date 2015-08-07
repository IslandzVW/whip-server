#include "StdAfx.h"
#include "AuthChallengeMsg.h"
#include "Random.h"

const short AuthChallengeMsg::PHRASE_SIZE = 7;
const whip::byte AuthChallengeMsg::PACKET_IDENTIFIER = 0; 

AuthChallengeMsg::AuthChallengeMsg(bool inbound)
{
	if (inbound) {
		_data.resize(MESSAGE_SIZE);

	} else {
		_data.push_back(PACKET_IDENTIFIER);
		this->generatePhrase();

	}
}

AuthChallengeMsg::~AuthChallengeMsg()
{
}

void AuthChallengeMsg::generatePhrase()
{
	for (short i = 0; i < PHRASE_SIZE; i++) {
		_data.push_back(Random::rand_int('0', 'z'));
	}
}

const whip::byte_array& AuthChallengeMsg::serialize() const
{
	return _data;
}

whip::byte_array& AuthChallengeMsg::getDataStorage()
{
	return _data;
}

std::string AuthChallengeMsg::getPhrase() const
{
	return std::string((char*)&_data[1], PHRASE_SIZE);
}
