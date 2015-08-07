#include "StdAfx.h"

#ifdef _WIN32

#include "WinDataFileImpl.h"

#include <boost/numeric/conversion/cast.hpp>

#include "AssetStorageError.h"

namespace iwvfs
{

	WinDataFileImpl::WinDataFileImpl()
		: _fileHandle(INVALID_HANDLE_VALUE)
	{
		
	}

	WinDataFileImpl::~WinDataFileImpl()
	{
		this->close();
	}

	void WinDataFileImpl::doThrow(const std::string& errorMessage)
	{
		throw AssetStorageError("[IWVFS][WIN32/64] " + errorMessage + " [" + _filePath.string() + "]", true);
	}

	void WinDataFileImpl::open(const boost::filesystem::path& filePath, FileMode mode)
	{
		DWORD access;
		if (mode == IDataFile::Read) {
			access = GENERIC_READ;
		} else {
			access = GENERIC_WRITE;
		}

		//always allow a read at the same time, never a write
		DWORD shareMode = FILE_SHARE_READ;

		DWORD creationDisposition;
		if (mode == IDataFile::Read) {
			//open existing on windows only opens the file if it exists
			creationDisposition = OPEN_EXISTING;
		} else {
			//open always creates the file if it doesnt exist, or opens it if it does
			creationDisposition = OPEN_ALWAYS;
		}

		_fileHandle = CreateFile(filePath.string().c_str(), access, shareMode, NULL, creationDisposition,
			FILE_ATTRIBUTE_NORMAL, NULL);

		if (_fileHandle == INVALID_HANDLE_VALUE) {
			throw AssetStorageError("[IWVFS][WIN32/64] Unable to open data file for writing [" + filePath.string() + "]", true);
		} else {
			_filePath = filePath;
		}
	}

	void WinDataFileImpl::close()
	{
		//CAN NOT THROW!  CALLED IN DESTRUCTOR
		if (_fileHandle != INVALID_HANDLE_VALUE) {
			CloseHandle(_fileHandle);
			_fileHandle = INVALID_HANDLE_VALUE;
		}
	}

	boost::int64_t WinDataFileImpl::size()
	{
		LARGE_INTEGER retsz;
		if (GetFileSizeEx(_fileHandle, &retsz)) {
			return boost::numeric_cast<boost::int64_t>(retsz.QuadPart);

		} else {
			this->doThrow("Unable to determine size of file");

			//unreachable
			return 0;
		}
	}

	void WinDataFileImpl::read(char* data, boost::uint32_t size)
	{
		DWORD bytesRead;
		if (ReadFile(_fileHandle, data, size, &bytesRead, NULL)) {
			if (bytesRead != size) {
				this->doThrow("Number of bytes read from file does not match requested size");
			}
			
		} else {
			this->doThrow("Unable to read data from file");
		}
	}

	void WinDataFileImpl::write(const char* data, boost::uint32_t size)
	{
		DWORD bytesWritten;
		if (WriteFile(_fileHandle, data, size, &bytesWritten, NULL)) {
			if (bytesWritten != size) {
				this->doThrow("Number of bytes written to file does not match requested size");
			}

		} else {
			this->doThrow("Unable to write to data file");
		}
	}

	void WinDataFileImpl::setFilePointerPosition(boost::int64_t position)
	{
		LARGE_INTEGER pos;
		pos.QuadPart = boost::numeric_cast<LONGLONG>(position);

		LARGE_INTEGER newPos;
		if (SetFilePointerEx(_fileHandle, pos, &newPos, FILE_BEGIN) != 0) {
			if (newPos.QuadPart != pos.QuadPart) {
				this->doThrow("File pointer position set does not match requested position");
			}
		} else {
			this->doThrow("Error when setting file pointer position");
		}
	}

	void WinDataFileImpl::seekg(boost::int64_t position)
	{
		this->setFilePointerPosition(position);
	}

	void WinDataFileImpl::seekp(boost::int64_t position)
	{
		this->setFilePointerPosition(position);
	}

}

#endif //_WIN32