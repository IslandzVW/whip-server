#pragma once

#include <iosfwd>
#include <boost/iostreams/tee.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

class FileAppenderImpl : public boost::noncopyable
{
private:
	std::string _fileName;
	std::ostream& _stdOut;

	boost::shared_ptr<std::stringstream> _buffer;
	boost::shared_ptr<boost::thread> _writerThread;

	boost::mutex _syncMutex;
	bool _stop;

	void worker();

	void doWrites();

public:
	FileAppenderImpl(const std::string& fileName, std::ostream& stdOut);
    std::streamsize write(const char* s, std::streamsize n);

	void shutDown();
};

class FileAppenderSink : public boost::iostreams::sink
{
private:
	FileAppenderImpl& _impl;

public:
	FileAppenderSink(FileAppenderImpl& impl);
	std::streamsize write(const char* s, std::streamsize n);
};

#define SAFELOG(logCommands) { boost::mutex::scoped_lock lock(AppLog::instance().getLock()); logCommands; }


/**
 * Provides simple logging to the application
 */
class AppLog
{
private:
	static AppLog* _instance;
	static std::ostream* _outStream;
	static FileAppenderSink* _errAppender;
	static FileAppenderImpl* _errAppenderImpl;
	static boost::iostreams::stream_buffer<FileAppenderSink>* _errStreamBuffer;
	static std::ostream* _errStream;

	boost::mutex _lock;

public:
	/**
	 * Sets the log stream for this logger
	 */
	static void SetOutStream(std::ostream* logStream);

	/**
	 * Returns the singleton instance
	 */
	static AppLog& instance();

	/**
	 * Shuts down the error writer thread
	 */
	static void ShutDown();

	/**
	 * CTOR
	 */
	AppLog();
	virtual ~AppLog();

	/**
	 * Returns the std out log stream
	 */
	std::ostream& out();

	/**
	 * Returns the error stream
	 */
	std::ostream& error();

	/**
	 * Returns the lock mutex
	 */
	boost::mutex& getLock();
};
