#include "StdAfx.h"
#include "Validator.h"

#include <boost/foreach.hpp>

Validator::Validator()
{
}

Validator::~Validator()
{
}

bool Validator::IsValidUUID(const std::string& uuid)
{
	//must be 32 characters
	if (uuid.length() != UUID_LEN) {
		return false;
	}

	//each character can only be 0-9 or the letters a-f
	BOOST_FOREACH(char ch, uuid) {
		if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f')) {
			return false;
		}
	}

	return true;
}
