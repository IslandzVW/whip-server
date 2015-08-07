#pragma once

#ifdef _WIN32

#include "IDataFile.h"

#include <string>
#include <boost/filesystem.hpp>
#include <windows.h>

namespace iwvfs
{
	/**
		IDataFile implementation for windows
	*/
	class WinDataFileImpl : public IDataFile
	{
	private:
		HANDLE _fileHandle;
		boost::filesystem::path _filePath;

		void doThrow(const std::string& errorMessage);
		void setFilePointerPosition(boost::int64_t position);

	public:
		WinDataFileImpl();
		virtual ~WinDataFileImpl();

		virtual void open(const boost::filesystem::path& filePath, FileMode mode);

		virtual void close();

		virtual boost::int64_t size();

		virtual void read(char* data, boost::uint32_t size);

		virtual void write(const char* data, boost::uint32_t size);

		virtual void seekg(boost::int64_t position);

		virtual void seekp(boost::int64_t position);
	};

}

#endif //_WIN32
