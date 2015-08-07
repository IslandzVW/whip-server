#pragma once

#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/cstdint.hpp>

namespace iwvfs
{
	/**
		Interface to a vfs data file
	*/
	class IDataFile
	{
	public:
		typedef boost::shared_ptr<IDataFile> ptr;

		/**
			File mode, read or write
		*/
		enum FileMode {
			Read,
			Write
		};

	public:
		virtual ~IDataFile();


		/**
			Opens a file for read or write
		*/
		virtual void open(const boost::filesystem::path& filePath, FileMode mode) = 0;

		/**
			Closes a file (this will also be called by the implementation on destruction)
		*/
		virtual void close() = 0;

		/**
			Returns the current size of the file
		*/
		virtual boost::int64_t size() = 0;

		/**
			Reads a block of data from the file
		*/
		virtual void read(char* data, boost::uint32_t size) = 0;

		/**
			Writes data to the file
		*/
		virtual void write(const char* data, boost::uint32_t size) = 0;

		/**
			Sets the current read pointer position (0 based offset)
		*/
		virtual void seekg(boost::int64_t position) = 0;

		/**
			Sets the current write pointer position (0 based offset)
		*/
		virtual void seekp(boost::int64_t position) = 0;
	};

}