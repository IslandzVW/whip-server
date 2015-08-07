#pragma once

namespace iwintramesh
{
	/**
	 * Message sent between servers for intramesh communications 
	 * including queries, responses and
	 */
	class IntraMeshMsg
	{
	public:
		typedef boost::shared_ptr<IntraMeshMsg> ptr;

		/**
		 * Type of request this is
		 */
		enum RequestType {
			/**
			 * Reuesting an asset from the mesh
			 */
			MESH_QUERY = 0,
			
			/**
			 * Response to an asset request over the mesh network
			 */
			MESH_RESPONSE = 1,

			/**
			 * A heartbeat signal
			 */
			STATUS_HEARTBEAT = 2
		};

		/**
		 * Flags sent with heartbeat indicating status
		 */
		enum HeartBeatFlags {
			/**
			 * The server is online
			 */
			HB_ONLINE = (1 << 0),

			/**
			 * The server is accepting read requests
			 */
			HB_READABLE = (1 << 1),

			/**
			 * The server is accepting write requests
			 */
			HB_WRITABLE = (1 << 2)
		};

		/**
		 * Result of a mesh request
		 */
		enum QueryResult {
			/**
			 * Requested asset was not found
			 */
			QR_NOTFOUND = 0,

			/**
			 * Requested asset was found
			 */
			QR_FOUND = 1,

			/**
			 * Error while trying to 
			 */
			QR_ERROR = 2
		};

		/**
		 * Maximum size of an intramesh packet
		 */
		const static short MAX_PACKET = 38;

		/**
		 * Size of a mesh query packet
		 */
		static const short MESH_QUERY_SZ = 33;

		/**
		 * Size of mesh response packet
		 */
		static const short MESH_RESPONSE_SZ = 34;

		/**
		 * Size of status heartbeat packet
		 */
		static const short STATUS_HEARTBEAT_SZ = 5;

		/**
		 * Location of the mesh response code
		 */
		static const short MESH_RESPONSE_CODE_LOC = 33;

		/**
		 * Location of the heartbeat flags
		 */
		static const short HEARTBEAT_FLAGS_LOC = 1;


	private:
		whip::byte_array _data;

	public:
		IntraMeshMsg();

		/**
		 * CTOR for mesh query
		 */
		IntraMeshMsg(const std::string& assetUUID);
		
		/**
		 * CTOR for mesh response
		 */
		IntraMeshMsg(const std::string& assetUUID, QueryResult result);

		/**
		 * CTOR for status heartbeat
		 */
		explicit IntraMeshMsg(int heartBeatMask);

		virtual ~IntraMeshMsg();

		/**
		 * Validates that this message is good
		 */
		bool validate() const;

		/**
		 * Returns the type of request this is
		 */
		RequestType getRequestType() const;

		/**
		 * Returns the result of a mesh query
		 */
		QueryResult getQueryResult() const;

		/**
		 * Returns the flags for the heartbeat
		 */
		int getHeartbeatFlags() const;

		/**
		 * Returns the UUID of the requested or queried asset
		 */
		std::string getAssetUUID() const;

		/**
		 * Returns the size of the packet
		 */
		short getMessageSize() const;

		/**
		 * Returns the internal storage array for use in an asio buffer
		 */
		whip::byte_array& getDataStorage();
	};

}