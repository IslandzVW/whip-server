#pragma once

#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <string>
#include <vector>
#include <utility>

namespace whip
{
	struct IntraMeshPeerEntry
	{
		std::string Host;
		unsigned short IntraMeshPort;
		unsigned short AssetServicePort;
	};

	/**
	 * Singleton class for accessing configuration settings
	 */
	class Settings
	{
	public:
		typedef boost::shared_ptr<boost::program_options::variables_map> VariablesMapPtr;

		const static int UUID_PATH_CHARS = 3;

	private:
		static const char* const SETTINGS_FILE;
		static Settings* _instance;
		VariablesMapPtr _vm;
		std::string _configFileName;
		mutable boost::mutex _lock;

	public:
		/**
		Retrieves the single instance of the settings object
		*/
		static Settings& instance() {
			if (! _instance) {
				_instance = new Settings(SETTINGS_FILE);
			}

			return *_instance;
		}

	public:
		Settings(const std::string& settingsFile);
		virtual ~Settings();

		const VariablesMapPtr config() const;

		/**
		 * Reloads the settings from the config file on disk
		 */
		void reload();

		/**
		 * Retrieves the peer list from the current settings
		 */
		std::vector<IntraMeshPeerEntry> intraMeshPeers() const;

		/**
		 * Retrieves the replication master from the current settings
		 */
		std::string replicationMaster() const;

		/**
		 * Retrieves the replication slave from the current settings
		 */
		std::string replicationSlave() const;

		/**
		 * Returns the given setting by name
		 */
		boost::program_options::variable_value get(const std::string& settingName) const;

	public:
		static std::pair<std::string, unsigned short> ParseHostPortPair(const std::string& hpp);
		static boost::asio::ip::tcp::endpoint ParseHostPortPairToTcpEndpoint(const std::string& hpp);
	};

}