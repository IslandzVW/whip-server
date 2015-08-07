#include "StdAfx.h"

#include "ClientRequestMsg.h"
#include "RequestError.h"

ClientRequestMsg::ClientRequestMsg()
: _header(HEADER_SIZE)
{
	
}
ClientRequestMsg::ClientRequestMsg(RequestType type, const std::string& uuid)
{
	_header.push_back((whip::byte) type);
	_header.insert(_header.end(), uuid.begin(), uuid.end());
	//four zero bytes for the size
	_header.insert(_header.end(), 4, 0);
}

ClientRequestMsg::ClientRequestMsg(Asset::ptr asset)
{
	_header.push_back((whip::byte) RT_PUT);

	std::string assetId(asset->getUUID());
	_header.insert(_header.end(), assetId.begin(), assetId.end());

	u_long sz = htonl(asset->getSize());

	//four zero bytes for the size
	_header.insert(_header.end(), 4, 0);
	//fill in the size
	*((unsigned int*)(&_header[DATA_SIZE_MARKER_LOC])) = sz;

	//insert the data
	_data = asset->getData();
}

ClientRequestMsg::~ClientRequestMsg()
{
}

void ClientRequestMsg::initDataStorageFromHeader()
{
	//read the size from the header
	unsigned long dataSize = this->getDataSize();
	_data.reset(new whip::byte_array(dataSize));
}

whip::byte_array& ClientRequestMsg::getHeaderData()
{
	return _header;
}

ClientRequestMsg::RequestType ClientRequestMsg::getType() const
{
	//make sure we actually have a valid header
	if (_header.size() == 0 || (_header[0] < RT_GET || _header[0] > RT_GET_DONTCACHE)) {
		throw RequestError("Bad request, invalid header on ClientRequest");
	}

	return static_cast<ClientRequestMsg::RequestType>(_header[0]);
}

std::string ClientRequestMsg::getUUID() const
{
	return std::string((char*) &_header[1], 32);
}

whip::byte_array_ptr ClientRequestMsg::getData()
{
	return _data;
}

unsigned int ClientRequestMsg::getDataSize() const
{
	return ntohl(*((unsigned int*)&_header[DATA_SIZE_MARKER_LOC]));
}