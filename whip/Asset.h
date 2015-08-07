#pragma once

#include <iosfwd>
#include <boost/shared_ptr.hpp>
#include "byte.h"

/**
Represents a single asset with data and metadata both in
storage and on the wire
*/
class Asset
{
private:
	whip::byte_array_ptr _data;

public:
	typedef boost::shared_ptr<Asset> ptr;

public:
	/**
		Constructs an asset with a data array pre allocated to the given size
	*/
	explicit Asset(unsigned int sizeHint);

	/**
		Constructs an asset from the given byte array	
	*/
	Asset(whip::byte_array_ptr data);
	
	virtual ~Asset();

	/**
		Returns the UUID of this asset from the first 32 bytes 
		of packet data
	*/
	std::string getUUID() const;

	/**
		Returns the size of the data array holding the asset
		data and metadata
	*/
	unsigned int getSize() const;

	/**
		Returns the internal data for this asset
	*/
	whip::byte_array_ptr getData() const;

	/**
		Returns whether or not this is a local asset
	*/
	bool isLocal() const;

	/**
		Returns the type of asset this is
	*/
	whip::byte getType() const;
};
