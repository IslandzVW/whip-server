#pragma once

#include <string>
#include <boost/shared_ptr.hpp>

class AssetStorageError : public std::runtime_error
{
private:
	bool _isCritical;

public:
	typedef boost::shared_ptr<AssetStorageError> ptr;

public:
	AssetStorageError(const AssetStorageError& error);
	AssetStorageError(const std::string& what);
	AssetStorageError(const std::string& what, bool isCritical);

	virtual ~AssetStorageError() throw();

	bool isCritical() const;
};
