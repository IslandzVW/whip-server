#include "StdAfx.h"
#include "IntraMeshMsg.h"

#include "Validator.h"

#include <boost/numeric/conversion/cast.hpp>

namespace iwintramesh
{
	IntraMeshMsg::IntraMeshMsg()
		: _data(IntraMeshMsg::MAX_PACKET)
	{
		
	}

	IntraMeshMsg::IntraMeshMsg(const std::string& assetUUID)
	{
		_data.reserve(IntraMeshMsg::MAX_PACKET);
		_data.push_back(MESH_QUERY);
		_data.insert(_data.begin() + 1, assetUUID.begin(), assetUUID.end());
		_data.resize(IntraMeshMsg::MAX_PACKET);
	}
	
	IntraMeshMsg::IntraMeshMsg(const std::string& assetUUID, QueryResult result)
	{
		_data.reserve(IntraMeshMsg::MAX_PACKET);
		_data.push_back(MESH_RESPONSE);
		_data.insert(_data.begin() + 1, assetUUID.begin(), assetUUID.end());
		_data.push_back(result);
		_data.resize(IntraMeshMsg::MAX_PACKET);
	}

	IntraMeshMsg::IntraMeshMsg(int heartBeatMask)
	{
		_data.reserve(IntraMeshMsg::MAX_PACKET);
		_data.push_back(STATUS_HEARTBEAT);
		_data.insert(_data.end(), 4, 0);
		*((unsigned int*)(&_data[HEARTBEAT_FLAGS_LOC])) = htonl(heartBeatMask);
		_data.resize(IntraMeshMsg::MAX_PACKET);
	}

	IntraMeshMsg::~IntraMeshMsg()
	{
		
	}

	IntraMeshMsg::RequestType IntraMeshMsg::getRequestType() const
	{
		return (IntraMeshMsg::RequestType) _data[0];
	}

	IntraMeshMsg::QueryResult IntraMeshMsg::getQueryResult() const
	{
		return (IntraMeshMsg::QueryResult) _data[MESH_RESPONSE_CODE_LOC];
	}

	int IntraMeshMsg::getHeartbeatFlags() const
	{
		return ntohl(*((int*)(&_data[HEARTBEAT_FLAGS_LOC])));
	}

	std::string IntraMeshMsg::getAssetUUID() const
	{
		std::string uuid;
		uuid.insert(uuid.begin(), _data.begin() + HEARTBEAT_FLAGS_LOC, _data.begin() + HEARTBEAT_FLAGS_LOC + Validator::UUID_LEN);

		return uuid;
	}

	whip::byte_array& IntraMeshMsg::getDataStorage()
	{
		return _data;
	}

	bool IntraMeshMsg::validate() const
	{
		switch (this->getRequestType())
		{
		case MESH_QUERY:
			if (_data.size() == IntraMeshMsg::MAX_PACKET && 
				Validator::IsValidUUID(this->getAssetUUID())) {
					return true;
			}
			break;

		case MESH_RESPONSE:
			if (_data.size() == IntraMeshMsg::MAX_PACKET &&
				Validator::IsValidUUID(this->getAssetUUID())) {
					return true;
			}
			break;

		case STATUS_HEARTBEAT:
			if (_data.size() == IntraMeshMsg::MAX_PACKET) return true;
			break;
		}

		return false;
	}
}