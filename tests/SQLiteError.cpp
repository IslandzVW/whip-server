#include "StdAfx.h"
#include "SQLiteError.h"

namespace iwvfs
{

	SQLiteError::SQLiteError(const std::string message, int errorCode, int extendedCode)
	: std::runtime_error(message), _errorCode(errorCode), _extendedCode(extendedCode)
	{
	}

	SQLiteError::~SQLiteError()
	{
	}

	/**
		Returns the SQLite error code
	*/
	int SQLiteError::getErrorCode() const
	{
		return _errorCode;
	}

	/**
		Returns the SQLite extended error code
	*/
	int SQLiteError::getExtendedCode() const
	{
		return _extendedCode;
	}

}