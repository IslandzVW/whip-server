#include "StdAfx.h"
#include "Asset.h"

#include <boost/numeric/conversion/cast.hpp>

Asset::Asset(unsigned int sizeHint)
: _data(new whip::byte_array(sizeHint))
{
}

Asset::Asset(whip::byte_array_ptr data)
: _data(data)
{

}

Asset::~Asset()
{
}

std::string Asset::getUUID() const
{
	return std::string((const char*) &((*_data)[0]), 32);
}

unsigned int Asset::getSize() const
{
	return boost::numeric_cast<unsigned int>(_data->size());
}

whip::byte_array_ptr Asset::getData() const
{
	return _data;
}

bool Asset::isLocal() const
{
	return (*_data)[33] == 1;
}

whip::byte Asset::getType() const
{
	return (*_data)[32];
}