#include "StdAfx.h"

#include "Settings.h"
#include "AssetServer.h"
#include "IntraMeshService.h"

#include <fstream>
//compiler complains about std::copy inside of boost
#pragma warning(disable:4996)
#include <boost/algorithm/string.hpp>
#pragma warning(default:4996)

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

namespace po = boost::program_options;
using namespace std;

namespace whip
{
	const char* const Settings::SETTINGS_FILE = "whip.cfg";
	Settings* Settings::_instance = 0;

	Settings::Settings(const std::string& configFileName)
		: _configFileName(configFileName)
	{
		this->reload();
	}

	Settings::~Settings()
	{
	}

	void Settings::reload()
	{
		const int TCP_BUFFSZ_DEFAULT = 65536;
		const int UDP_BUFFSZ_DEFAULT = 32768;
		const int DEFAULT_PULL_REPL_FREQUENCY = 720; //12 hrs
		const unsigned int DEFAULT_PULL_REPL_BATCH_SIZE = 1;

		//setup options
		po::options_description desc("Allowed options");
		desc.add_options()
			("port", po::value<unsigned short>()->default_value(AssetServer::DEFAULT_PORT), "TCP port to listen for client connections")
			("password", po::value<std::string>(), "Password for connection")
			("cache_enabled", po::value<bool>(), "Enable memory cache")
			("cache_size", po::value<unsigned int>(), "Maximum size of the asset cache in GB")
			("disk_storage_backend", po::value<std::string>(), "The name of the persistent storage backend to use")
			("disk_storage_root", po::value<std::string>(), "Where the Disk Storage Backend should store the asset files")
			("secondary_storage_root", po::value<std::string>()->default_value(""), "A second place for the disk storage backend to look for asset files")
			("migration", po::value<std::string>()->default_value("none"), "The name of the migration to perform (or 'none' for no migration)")
			("allow_purge", po::value<bool>(), "Whether or not to allow the purge operation")
			("debug", po::value<bool>(), "Enable debugging")
			("intramesh_peers", po::value<std::string>(), "Peers to contact on intramesh heartbeat and for intramesh queries")
			("intramesh_port", po::value<unsigned short>()->default_value(iwintramesh::IntraMeshService::DEFAULT_PORT), "Port to listen on for intramesh queries")
			("is_writable", po::value<bool>(), "Whether or not this server is accepting writes")
			("tcp_bufsz", po::value<int>()->default_value(TCP_BUFFSZ_DEFAULT), "The size of the recieve and send buffers for TCP connections")
			("udp_bufsz", po::value<int>()->default_value(UDP_BUFFSZ_DEFAULT), "The size of the recieve and send buffers for UDP connections")
			("replication_master",  po::value<std::string>()->default_value("none"), "Hostname for the master server this server is replicating from")
			("pull_replication_frequency", po::value<int>()->default_value(DEFAULT_PULL_REPL_FREQUENCY), "Frequency which the master server will be checked for changes in minutes")
			("pull_replication_batch_size", po::value<unsigned int>()->default_value(DEFAULT_PULL_REPL_BATCH_SIZE), "Number of asset requests to make in a single batch. Higher means faster replication at the cost of master server load")
			("pull_replication_start_at", po::value<int>()->default_value(0), "Asset id prefix that pull replication should start from")
			("replication_slave", po::value<std::string>()->default_value("none"), "Slave server that will take change updates from this master")
		;
		

		//try to load the config file
		boost::mutex::scoped_lock lock(_lock);
		std::ifstream configFile(_configFileName.c_str());

		if (configFile.good()) {
			_vm.reset(new po::variables_map());
			po::store(po::parse_config_file(configFile, desc), *_vm);    
		} else {
			throw std::runtime_error("Could not open configuration file '" + _configFileName + "'");
		}

		po::notify(*_vm);
	}

	const Settings::VariablesMapPtr Settings::config() const
	{
		return _vm;
	}

	std::string Settings::replicationMaster() const
	{
		boost::mutex::scoped_lock lock(_lock);

		string masterSetting = (*_vm)["replication_master"].as<string>();
		
		if (masterSetting == "none" || masterSetting == "") {
			return string("");
		}

		return masterSetting;
	}

	std::string Settings::replicationSlave() const
	{
		boost::mutex::scoped_lock lock(_lock);

		string slaveSetting = (*_vm)["replication_slave"].as<string>();
		
		if (slaveSetting == "none" || slaveSetting == "") {
			return string("");
		}

		return slaveSetting;
	}
	
	std::vector<IntraMeshPeerEntry> Settings::intraMeshPeers() const
	{
		boost::mutex::scoped_lock lock(_lock);
		vector<IntraMeshPeerEntry> peers;

		string peerSetting = (*_vm)["intramesh_peers"].as<string>();
		
		if (peerSetting == "none") {
			return peers;
		}
		
		vector<string> hosts;
		boost::split(hosts, peerSetting, boost::is_any_of(","));
		
		//HOST:IM_PORT:AS_PORT,HOST:IM_PORT:AS_PORT
		BOOST_FOREACH(const string& host, hosts)
		{
			vector<string> hostAndPorts;
			boost::split(hostAndPorts, host, boost::is_any_of(":"));

			if (hostAndPorts.size() == 3) {
				IntraMeshPeerEntry entry;
				entry.Host = hostAndPorts[0];
				entry.IntraMeshPort = boost::lexical_cast<unsigned short>(hostAndPorts[1]);
				entry.AssetServicePort = boost::lexical_cast<unsigned short>(hostAndPorts[2]);

				peers.push_back(entry);
			}
		}

		return peers;
	}

	boost::program_options::variable_value Settings::get(const std::string& settingName) const
	{
		boost::mutex::scoped_lock lock(_lock);
		boost::program_options::variable_value val = (*_vm)[settingName];

		return val;
	}

	std::pair<std::string, unsigned short> Settings::ParseHostPortPair(const std::string& hpp)
	{
		vector<string> hostAndPorts;
		boost::split(hostAndPorts, hpp, boost::is_any_of(":"));

		if (hostAndPorts.size() != 2) {
			throw std::runtime_error("ParseHostPortPair failed, entry must be in the format HOST:PORT");
		}

		return std::make_pair(hostAndPorts[0], boost::lexical_cast<unsigned short>(hostAndPorts[1]));
	}

	boost::asio::ip::tcp::endpoint Settings::ParseHostPortPairToTcpEndpoint(const std::string& hpp)
	{
		std::pair<std::string, unsigned short> hostAndPorts(ParseHostPortPair(hpp));

		boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string(hostAndPorts.first), hostAndPorts.second);
		return ep;
	}
}