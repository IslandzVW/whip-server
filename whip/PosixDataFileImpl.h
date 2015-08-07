#pragma once

#include "IDataFile.h"

#include <string>
#include <boost/filesystem.hpp>

namespace iwvfs
{

	class PosixDataFileImpl : public IDataFile
	{
	private:
		int _fd;
		boost::filesystem::path _filePath;
		
		void doSeek(boost::int64_t position);

	public:
		PosixDataFileImpl();
		virtual ~PosixDataFileImpl();

		/**
			Opens a file for read or write
		*/
		virtual void open(const boost::filesystem::path& filePath, FileMode mode);

		/**
			Closes a file (this will also be called by the implementation on destruction)
		*/
		virtual void close();

		/**
			Returns the current size of the file
		*/
		virtual boost::int64_t size();

		/**
			Reads a block of data from the file
		*/
		virtual void read(char* data, boost::uint32_t size);

		/**
			Writes data to the file
		*/
		virtual void write(const char* data, boost::uint32_t size);

		/**
			Sets the current read pointer position (0 based offset)
		*/
		virtual void seekg(boost::int64_t position);

		/**
			Sets the current write pointer position (0 based offset)
		*/
		virtual void seekp(boost::int64_t position);
	};

}

