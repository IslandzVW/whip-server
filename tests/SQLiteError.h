#pragma once

#include <stdexcept>

namespace iwvfs
{
	/**
		An error thrown by the sqlite database engine
	*/
	class SQLiteError : public std::runtime_error
	{
	private:
		int _errorCode;
		int _extendedCode;

	public:
		SQLiteError(const std::string message, int errorCode, int extendedCode);
		virtual ~SQLiteError();

		/**
			Returns the SQLite error code
		*/
		int getErrorCode() const;

		/**
			Returns the SQLite extended error code
		*/
		int getExtendedCode() const;
	};

}