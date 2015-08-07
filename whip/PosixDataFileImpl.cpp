#include "StdAfx.h"

#ifdef __linux__

#include "PosixDataFileImpl.h"
#include "AssetStorageError.h"

#include <fcntl.h>
#include <unistd.h>

namespace iwvfs
{

	PosixDataFileImpl::PosixDataFileImpl() : _fd(0)
	{
	}


	PosixDataFileImpl::~PosixDataFileImpl()
	{
		try {
			this->close();
		} catch (...) {
		}
	}

	void PosixDataFileImpl::open(const boost::filesystem::path& filePath, FileMode mode)
	{
		int openFlags;
		int creationMode;

		if (mode == IDataFile::Read) {
			openFlags = O_RDONLY;
			creationMode = 0;
		} else {
			openFlags = O_WRONLY | O_CREAT;
			creationMode = (S_IWUSR | S_IRUSR) | (S_IWGRP | S_IRGRP);
		}

		_fd = ::open(filePath.string().c_str(), openFlags, creationMode);
		if (_fd == -1) {
			throw AssetStorageError("[IWVFS][POSIX] Unable to open data file for writing [" + _filePath.string() + "]", true);
		}

		_filePath = filePath;
	}

	void PosixDataFileImpl::close()
	{
		::close(_fd);
	}

	boost::int64_t PosixDataFileImpl::size()
	{
		 struct stat sb;

		 if (fstat(_fd, &sb) == -1) {
			 throw AssetStorageError("[IWVFS][POSIX] Unable to stat data file to retrieve size [" + _filePath.string() + "]", true);
		 }

		 return (boost::int64_t) sb.st_size;
	}

	void PosixDataFileImpl::read(char* data, boost::uint32_t size)
	{
		ssize_t bytesRead = ::read(_fd, data, size);

		if (bytesRead == -1) {
			throw AssetStorageError("[IWVFS][POSIX] Error while reading from data file [" + _filePath.string() + "]", true);
		}
		
		if (bytesRead != size) {
			throw AssetStorageError("[IWVFS][POSIX] Short read encountered while reading from data file [" + _filePath.string() + "]", true);
		}
	}

	void PosixDataFileImpl::write(const char* data, boost::uint32_t size)
	{
		ssize_t bytesWritten = ::write(_fd, data, size);

		if (bytesWritten == -1) {
			throw AssetStorageError("[IWVFS][POSIX] Error while writing to data file [" + _filePath.string() + "]", true);
		}
		
		if (bytesWritten != size) {
			throw AssetStorageError("[IWVFS][POSIX] Short write encountered while writing to data file [" + _filePath.string() + "]", true);
		}
	}

	void PosixDataFileImpl::doSeek(boost::int64_t position)
	{
		if (lseek(_fd, (off_t) position, SEEK_SET) == (off_t) -1) {
			throw AssetStorageError("[IWVFS][POSIX] Error while seeking file [" + _filePath.string() + "]", true);
		}
	}

	void PosixDataFileImpl::seekg(boost::int64_t position)
	{
		this->doSeek(position);
	}

	void PosixDataFileImpl::seekp(boost::int64_t position)
	{
		this->doSeek(position);
	}

}

#endif

