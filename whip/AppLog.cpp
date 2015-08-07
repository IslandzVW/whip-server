#include "StdAfx.h"
#include "AppLog.h"
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/iostreams/concepts.hpp> 
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/device/file.hpp>

#include <iostream>
#include <sstream>
#include <fstream>

using namespace boost::posix_time;

FileAppenderImpl::FileAppenderImpl(const std::string& fileName, std::ostream& stdOut)
	: _fileName(fileName), _stdOut(stdOut), _buffer(new std::stringstream()), _stop(false)
{
	_writerThread.reset(new boost::thread(boost::bind(&FileAppenderImpl::worker, this)));
}

void FileAppenderImpl::worker()
{
	while (! _stop) {

		try {

			this->doWrites();
		
			boost::this_thread::sleep(boost::posix_time::seconds(5));
		} catch(...) {
		}

	}

	this->doWrites();
}

void FileAppenderImpl::doWrites()
{
	boost::mutex::scoped_lock lock(_syncMutex);

	if (_buffer->str().length() > 0) {
		std::ofstream file(_fileName.c_str(), std::ios_base::app);
		if (file.good()) {
			file << _buffer->str();
			_buffer->str("");
		}
	}		
}

std::streamsize FileAppenderImpl::write(const char* s, std::streamsize n)
{
	_stdOut.write(s, n);

	boost::mutex::scoped_lock lock(_syncMutex);
	_buffer->write(s, n);
	return n;
}

void FileAppenderImpl::shutDown()
{
	_stop = true;

	if (_writerThread->joinable()) {
		_writerThread->join();
	}
}

FileAppenderSink::FileAppenderSink(FileAppenderImpl& impl)
	: _impl(impl)
{

}

std::streamsize FileAppenderSink::write(const char* s, std::streamsize n)
{
	return _impl.write(s, n);
}

AppLog* AppLog::_instance;
std::ostream* AppLog::_outStream;
FileAppenderSink* AppLog::_errAppender;
FileAppenderImpl* AppLog::_errAppenderImpl;
std::ostream* AppLog::_errStream;
boost::iostreams::stream_buffer<FileAppenderSink>* AppLog::_errStreamBuffer;

void AppLog::SetOutStream(std::ostream* logStream)
{
	_outStream = logStream;
	
	_errAppenderImpl = new FileAppenderImpl("errorlog.txt", *_outStream);
	_errAppender = new FileAppenderSink(*_errAppenderImpl);
	_errStreamBuffer = new boost::iostreams::stream_buffer<FileAppenderSink>(*_errAppender);
	_errStream = new std::ostream(_errStreamBuffer);
}

void AppLog::ShutDown()
{
	_errAppenderImpl->shutDown();
}

AppLog& AppLog::instance() 
{
	if (! _instance) {
		_instance = new AppLog();
	}

	return *_instance;
}

AppLog::AppLog()
{
}

AppLog::~AppLog()
{
	
}

std::ostream& AppLog::out()
{
	ptime now(second_clock::local_time());
	(*AppLog::_outStream) << "[" << now << "] ";
	return *AppLog::_outStream;
}

std::ostream& AppLog::error()
{
	ptime now(second_clock::local_time());
	(*AppLog::_errStream) << "[" << now << "] |ERR| ";
	return *AppLog::_errStream;
}

boost::mutex& AppLog::getLock()
{
	return _lock;
}
