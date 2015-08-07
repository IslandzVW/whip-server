#include "StdAfx.h"
#include "AuthStatusMsg.h"

const whip::byte AuthStatusMsg::PACKET_IDENTIFIER = 1;
const short AuthStatusMsg::MESSAGE_SIZE = 2;

AuthStatusMsg::AuthStatusMsg()
: _messageData(MESSAGE_SIZE)
{
}

AuthStatusMsg::AuthStatusMsg(AuthStatus authStatus)
{
	_messageData.push_back(AuthStatusMsg::PACKET_IDENTIFIER);
	_messageData.push_back(static_cast<whip::byte>(authStatus));
}

AuthStatusMsg::~AuthStatusMsg()
{
}

const whip::byte_array& AuthStatusMsg::serialize() const
{
	return _messageData;
}

whip::byte_array& AuthStatusMsg::getDataStorage()
{
	return _messageData;
}

AuthStatus AuthStatusMsg::getAuthStatus() const
{
	return (AuthStatus) _messageData[1];
}

bool AuthStatusMsg::validate() const
{
	if (_messageData.size() != MESSAGE_SIZE) {
		return false;
	}

	if (_messageData[0] != PACKET_IDENTIFIER) {
		return false;
	}

	if (_messageData[1] != AS_AUTH_SUCCESS &&
		_messageData[1] != AS_AUTH_FAILURE)
	{
		return false;
	}

	return true;
}
