#pragma once

#include "Asset.h"
#include "AssetStorageError.h"

#include <boost/function.hpp>
#include <string>

typedef boost::function<void(Asset::ptr, bool, AssetStorageError::ptr)> AsyncGetAssetCallback;
typedef boost::function<void(bool, AssetStorageError::ptr)> AsyncStoreAssetCallback;
typedef boost::function<void(bool, AssetStorageError::ptr)> AsyncAssetPurgeCallback;

