#pragma once

#include "IAsyncStorageBackend.h"
#include "IntraMeshMsg.h"

#include <boost/shared_ptr.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/noncopyable.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <queue>

#include <string>

namespace iwintramesh
{
	class IntraMeshService;

	struct IQCSendItem
	{
		IntraMeshMsg::ptr message;
		boost::function<void(const boost::system::error_code&, size_t)> callBack;
	};

	/**
	 * A client that sends intramesh messages/queries to this server
	 */
	class IntraMeshQueryClient : public boost::enable_shared_from_this<IntraMeshQueryClient>, public boost::noncopyable
	{
	public:
		typedef boost::shared_ptr<IntraMeshQueryClient> ptr;

	private:
		boost::asio::io_service& _ioService;
		IntraMeshService& _meshService;
		IAsyncStorageBackend::ptr _diskBackend;
		boost::asio::ip::tcp::socket _conn;
		bool _debugging;

		bool _sendActive;
		bool _recvActive;

		std::queue<IQCSendItem> _pendingSends;

		boost::asio::ip::tcp::endpoint _connectedEp;

		/**
		 * Prints an error to the error log
		 */
		void printIoError(const std::string& method, const boost::system::error_code& error);

		/**
		 * Listens for an incoming mesh message
		 */
		void listenForMeshMessage();

		/**
		 * Handles receiving an intramesh message
		 */
		void onMeshMessageReceived(const boost::system::error_code& error,
			size_t bytesRcvd, IntraMeshMsg::ptr inboundMsg);

		/**
		 * Handles receiving an intramesh message
		 */
		void processIntrameshMessage(IntraMeshMsg::ptr inboundMsg);

		/**
		 * Handles an incoming query
		 */
		void processMeshQuery(IntraMeshMsg::ptr inboundMsg);

		/**
		 * Sends to the remote server that the given asset was found
		 */
		void sendAssetFound(IntraMeshMsg::ptr inboundMsg);

		/**
		 * Sends to the remote server that the given asset was not found
		 */
		void sendAssetNotFound(IntraMeshMsg::ptr inboundMsg);

		/**
		 * Sends to the remote server that there was an error testing for the asset
		 */
		void sendAssetError(IntraMeshMsg::ptr inboundMsg);

		void sendQueryResponse(IntraMeshMsg::ptr outbound);

		void onAfterSendQueryResponse(const boost::system::error_code& error,
			size_t bytesSent, IntraMeshMsg::ptr outbound);

		/**
		 * Returns true if there is a waiting recv or a pending send
		 */
		bool connectionActive();

		/**
		 * If there is a send waiting in the send queue, this processes it
		 * and sends the result
		 */
		void processQueuedSend();

		void onAfterSendHeartbeat(const boost::system::error_code& error,
			size_t bytesSent, IntraMeshMsg::ptr outbound);

	public:
		IntraMeshQueryClient(boost::asio::io_service& ioService, 
			IntraMeshService& meshService,
			IAsyncStorageBackend::ptr diskBackend,
			bool debugging);

		virtual ~IntraMeshQueryClient();

		/**
		 * Returns the connection associated with this client
		 */
		boost::asio::ip::tcp::socket& getConn();

		/**
		 * Closes the connection from the client
		 * (a call to this function will cause this object to be deleted when it
		 *  is removed from the intramesh service, therefore after this call this
		 *  object is invalid)
		 */
		void close();

		/**
		 * Sends a heartbeat to this client
		 */
		void sendHeartbeat(int flags);

		/**
		 * Starts listening for traffic to service
		 */
		void start();

		/**
		 * Returns the endpoint we're connected to
		 */
		const boost::asio::ip::tcp::endpoint& getEndpoint() const;
	};	

}