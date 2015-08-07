#pragma once

#include <vector>
#include <boost/shared_ptr.hpp>

namespace whip 
{
	typedef unsigned char byte;
	typedef std::vector<whip::byte> byte_array;
	typedef boost::shared_ptr<byte_array> byte_array_ptr;
}