#include "StdAfx.h"

#include "AssetStorageError.h"

AssetStorageError::AssetStorageError(const std::string& message)
	: std::runtime_error(message), _isCritical(false)
{
}

AssetStorageError::AssetStorageError(const std::string& message, bool isCritical)
	: std::runtime_error(message), _isCritical(isCritical)
{
}

AssetStorageError::AssetStorageError(const AssetStorageError& error)
	: std::runtime_error(error.what()), _isCritical(error._isCritical)
{
}

AssetStorageError::~AssetStorageError() throw()
{
}

bool AssetStorageError::isCritical() const
{
	return _isCritical;
}
