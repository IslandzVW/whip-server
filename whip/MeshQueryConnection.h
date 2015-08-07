#pragma once

#include "IntraMeshMsg.h"

#include <boost/function.hpp>
#include <boost/system/error_code.hpp>
#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

#include <set>
#include <map>
#include <string>
#include <queue>

namespace iwintramesh
{
	/**
	 * Represents a connection to a mesh query service on a remote server
	 */
	class MeshQueryConnection : public boost::noncopyable
	{
	public:
		typedef boost::shared_ptr<MeshQueryConnection> ptr;

	private:
		/**
		 * Socket to send out a mesh query on
		 */
		boost::asio::ip::tcp::socket _meshQuerySocket;

		/**
		 * The endpoint we connect to for service
		 */
		boost::asio::ip::tcp::endpoint _endPoint;

		/**
		 * Function to call when this socket is disconnected
		 */
		boost::function<void()> _disconnectCallback;

		/**
		 * Function to call when this socket receives a new query response
		 */
		boost::function<void(IntraMeshMsg::ptr)> _messageRcvdCallback;

		/**
		 * Whether or not we have a connection to the mesh query service
		 */
		bool _isConnected;

		/**
		 * Whether we are currently awaiting data in a receive call
		 */
		bool _recvActive;

		/**
		 * Whether we are currently sending data to the server
		 */
		bool _sendActive;


		/**
		 * Queue holding queries to be sent out
		 */
		std::queue<std::string> _sendQueue;

		/**
		 * Whether or not the close callback has already been fired
		 */
		bool _disconnectCallbackFired;




		void onAfterMeshServiceConnect(const boost::system::error_code& error,
			boost::function<void(const boost::system::error_code& error)> connectCallback);

		void listenForMessage();

		void onAfterMeshMessageReceived(const boost::system::error_code& error,
			size_t bytesRcvd, IntraMeshMsg::ptr inboundMsg);

		void sendQuery(const std::string& uuid);

		void onAfterMeshMessageWritten(const boost::system::error_code& error,
			size_t bytesRcvd, IntraMeshMsg::ptr inboundMsg);

		void printIoError(const std::string& method, const boost::system::error_code& error);

		void writeNextMessage();


	public:
		MeshQueryConnection(boost::asio::io_service& ioService,
			const boost::asio::ip::tcp::endpoint& endPoint,
			boost::function<void()> disconnectCallback,
			boost::function<void(IntraMeshMsg::ptr)> dataRcvdCallback);

		virtual ~MeshQueryConnection();

		void connect(boost::function<void(const boost::system::error_code& error)> connectCallback);

		bool isConnected() const;

		void close();

		void search(const std::string& uuid);
	};

}
