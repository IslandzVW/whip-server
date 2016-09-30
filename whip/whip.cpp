// whip.cpp : Defines the entry point for the console application.
//

#include "StdAfx.h"

#include "AssetServer.h"
#include "Random.h"

#include <boost/function.hpp>
#include <boost/thread/mutex.hpp>
#include <iostream>
#include <fstream>


const std::string& VERSION = "5.01";
boost::asio::io_service* AppIoService;

boost::function0<void> console_ctrl_function;

boost::mutex ShutdownMutex;
boost::condition_variable ShutdownCond;

#ifdef _WIN32
BOOL WINAPI console_ctrl_handler(DWORD ctrl_type)
{
	switch (ctrl_type)
	{
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		SAFELOG(AppLog::instance().out() << "Clean shutdown initiated" << std::endl);
		
		{
			boost::unique_lock<boost::mutex> lock(ShutdownMutex);
			AppIoService->post(console_ctrl_function);
			ShutdownCond.wait(lock);
		}
		return TRUE;
	default:
		return FALSE;
	}
}
#endif

int main(int argc, char* argv[])
{
	using namespace std;
	
	//set the log stream output to console
	AppLog::SetOutStream(&cout);

	AppLog::instance();

	SAFELOG(AppLog::instance().out() << "InWorldz WHIP Asset Server " << VERSION << endl);
	SAFELOG(AppLog::instance().out() << "Build: " << __DATE__ " " __TIME__ << endl << endl);

	try {
		//init random number generator
		Random::initialize();

		//launch services
		
		AppIoService = new boost::asio::io_service();
		
		//read setting for port number
		unsigned short port = whip::Settings::instance().get("port").as<unsigned short>();
		bool debugging = whip::Settings::instance().get("debug").as<bool>();

		if (debugging) SAFELOG(AppLog::instance().out() << "DEBUGGING ENABLED" << endl << endl);

		AssetServer server(*AppIoService, port);
#ifdef _WIN32
		console_ctrl_function = boost::bind(&AssetServer::stop, &server);
		SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#endif
		server.start();
		AppIoService->run();

	} catch (const std::exception& e) {
		SAFELOG(AppLog::instance().error() 
			<< "Critical error: Unhandled exception: " << e.what() 
			<< ".  Server is shutting down"
			<< endl);
		std::string in;
		cin >> in;
	}

	delete AppIoService;

	boost::unique_lock<boost::mutex> lock(ShutdownMutex);
	ShutdownCond.notify_all();

	SAFELOG(AppLog::instance().out() << "Shutdown complete" << std::endl);

	AppLog::ShutDown();

	return 0;
}

