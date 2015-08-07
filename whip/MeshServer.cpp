#include "StdAfx.h"
#include "MeshServer.h"
#include "AppLog.h"
#include "IntraMeshMsg.h"
#include "ClientRequestMsg.h"
#include "ServerResponseMsg.h"
#include "AssetStorageError.h"
#include "AssetSearch.h"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <string>
#include <sstream>

using namespace boost::posix_time;
using boost::asio::ip::tcp;
using boost::asio::ip::udp;
using std::string;

namespace iwintramesh
{
	MeshServer::MeshServer(const boost::asio::ip::tcp::endpoint& listeningEndpoint,
		const boost::asio::ip::tcp::endpoint& assetServiceEndpoint, boost::asio::io_service& ioService,
		int initialFlags)
	: _lastHeartbeat(second_clock::local_time()), 
		_meshQueryEndpoint(listeningEndpoint),
		_assetServiceEndpoint(assetServiceEndpoint),
		_ioService(ioService),
		_serviceSocket(_ioService),
		_lastHeartbeatFlags(initialFlags),
		_assetConnectionEstablished(false),
		_meshQueryConnectionEstablished(false),
		_numConnectResultsRcvd(0),
		_sendActive(false),
		_recvActive(false)
	{
		_meshQueryConnection.reset(
			new MeshQueryConnection(ioService, _meshQueryEndpoint, 
				boost::bind(&MeshServer::queryEndpointClosed, this),
				boost::bind(&MeshServer::onQueryAnswerRcvd, this, _1)));
	}

	MeshServer::~MeshServer()
	{
		try {
			_safeKillCallback.clear();
			this->close();
		} catch (...) {
		}
	}
	
	void MeshServer::queryEndpointClosed()
	{
		_meshQueryConnectionEstablished = false;
		this->close();
	}

	void MeshServer::close()
	{
		_serviceSocket.close();
		_meshQueryConnection->close();

		_assetConnectionEstablished = false;

		//kill all transfer requests
		while (_transferRequestQueue.size() > 0) {
			PendingTransferRequest& req(_transferRequestQueue.front());

			SAFELOG(AppLog::instance().error() 
				<< "Canceling transfer request for asset " << req.get<0>()
				<< "due to mesh server socket closure"
				<< std::endl);

			(req.get<1>())(Asset::ptr());

			//remember after this line req is invalid!
			_transferRequestQueue.pop();
		}

		//kill all sent callbacks waiting
		while (_assetCallbackQueue.size() > 0) {
			PendingTransferRequest& req(_assetCallbackQueue.front());

			SAFELOG(AppLog::instance().error() 
				<< "Canceling callback for asset " << req.get<0>()
				<< "due to mesh server socket closure"
				<< std::endl);

			(req.get<1>())(Asset::ptr());

			//remember after this line req is invalid!
			_assetCallbackQueue.pop();
		}

		//kill all searches
		BOOST_FOREACH(AssetIdToSearchMap::value_type& kvp, _currentSearches)
		{
			BOOST_FOREACH(AssetSearch::wptr wsearch, kvp.second)
			{
				if (AssetSearch::ptr search = wsearch.lock()) {
					search->onSearchResult(shared_from_this(), kvp.first, false);
				}
			}
		}

		this->checkSafeKill();
	}

	bool MeshServer::isTimedOut() const
	{
		if (_assetConnectionEstablished) {
			return second_clock::local_time() - _lastHeartbeat > seconds(HEARTBEAT_TIMEOUT);
		} else {
			return true;
		}
	}

	const boost::asio::ip::tcp::endpoint& MeshServer::getQueryEndpoint() const
	{
		return _meshQueryEndpoint;
	}

	void MeshServer::updateHeartbeat(int flags)
	{
		_lastHeartbeat = second_clock::local_time();
		_lastHeartbeatFlags = flags;
	}

	int MeshServer::getHeartbeatFlags() const
	{
		return _lastHeartbeatFlags;
	}

	void MeshServer::connect(ConnectCallback connectCallback)
	{
		_connectCallback = connectCallback;

		_serviceSocket.async_connect(_assetServiceEndpoint,
			boost::bind(&MeshServer::onConnect, this,
			  boost::asio::placeholders::error));

		_meshQueryConnection->connect(boost::bind(&MeshServer::onQueryConnectionConnectResult, this, _1));
	}

	void MeshServer::onQueryConnectionConnectResult(const boost::system::error_code& error)
	{
		if (! error) {
			_meshQueryConnectionEstablished = true;
		} 

		this->onConnectionResult();
	}

	void MeshServer::onConnectionResult()
	{
		_numConnectResultsRcvd++;
		if (_numConnectResultsRcvd == TOTAL_CONNECTION_COUNT) {
			if (_meshQueryConnectionEstablished && _assetConnectionEstablished) {
				this->listenForAssetData();
			} 

			_connectCallback(shared_from_this(), _meshQueryConnectionEstablished && _assetConnectionEstablished);
			//after connectCallback is invoked this object may no longer exist. no code below this point
		}
	}

	void MeshServer::onMeshServiceConnect(const boost::system::error_code& error)
	{
		if (! error) {
			SAFELOG(AppLog::instance().out() 
				<< "[IWINTRAMESH] Connection established to query service on intramesh server: "
				<< _assetServiceEndpoint
				<< std::endl);



		} else {
			SAFELOG(AppLog::instance().error() 
				<< "[IWINTRAMESH] Unable to make connection to asset service on intramesh server: "
				<< _assetServiceEndpoint
				<< std::endl);
			
			_assetConnectionEstablished = false;
			this->onConnectionResult();
		}
	}

	void MeshServer::onConnect(const boost::system::error_code& error)
	{
		if (! error) {
			SAFELOG(AppLog::instance().out() 
				<< "[IWINTRAMESH] Connection established to asset service on intramesh server: "
				<< _assetServiceEndpoint
				<< std::endl);

			whip::Settings::VariablesMapPtr config(whip::Settings::instance().config());
			int BUF_SZ = whip::Settings::instance().get("tcp_bufsz").as<int>();

			_serviceSocket.set_option(boost::asio::socket_base::send_buffer_size(BUF_SZ));
			_serviceSocket.set_option(boost::asio::socket_base::receive_buffer_size(BUF_SZ));

			//begin authentication, wait for receive challenge
			_inboundAuthChallenge.reset(new AuthChallengeMsg(true));

			boost::asio::async_read(_serviceSocket,
				boost::asio::buffer(_inboundAuthChallenge->getDataStorage(), AuthChallengeMsg::MESSAGE_SIZE),
				boost::bind(
				  &MeshServer::onRecvChallenge, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));

		} else {
			SAFELOG(AppLog::instance().error() 
				<< "[IWINTRAMESH] Unable to make connection to asset service on intramesh server: "
				<< _assetServiceEndpoint
				<< std::endl);
			
			_assetConnectionEstablished = false;
			this->onConnectionResult();
		}
	}

	void MeshServer::sendChallengeResponse()
	{
		AuthResponseMsg::ptr respMessage(new AuthResponseMsg(_inboundAuthChallenge->getPhrase(), 
			whip::Settings::instance().get("password").as<string>()));

		boost::asio::async_write(_serviceSocket,
			boost::asio::buffer(respMessage->getDataStorage(), AuthResponseMsg::MESSAGE_SIZE),
			boost::bind(&MeshServer::onChallengeResponseWrite, this,
				boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred,
				respMessage));
	}

	void MeshServer::onAuthenticationStatus(const boost::system::error_code& error, size_t bytesRcvd,
			AuthStatusMsg::ptr authStatus)
	{
		if (!error && bytesRcvd > 0) {
			if (! authStatus->validate()) {
				SAFELOG(AppLog::instance().error() 
					<< "[IWINTRAMESH] Unable to authenticate with asset service on intramesh server: "
					<< _assetServiceEndpoint
					<< ": The server's authentication response was invalid"
					<< std::endl);
			
				_assetConnectionEstablished = false;
				this->onConnectionResult();
				return;
			}

			if (authStatus->getAuthStatus() != AS_AUTH_SUCCESS) {
				SAFELOG(AppLog::instance().error() 
					<< "[IWINTRAMESH] Unable to authenticate with asset service on intramesh server: "
					<< _assetServiceEndpoint
					<< ": Invalid credentials"
					<< std::endl);
			
				_assetConnectionEstablished = false;
				this->onConnectionResult();
				return;
			}

			_assetConnectionEstablished = true;
			this->onConnectionResult();

		} else {
			SAFELOG(AppLog::instance().error() 
				<< "[IWINTRAMESH] Unable to authenticate with asset service on intramesh server: "
				<< _assetServiceEndpoint
				<< ": "
				<< error.message()
				<< std::endl);
			
			_assetConnectionEstablished = false;
			this->onConnectionResult();
		}
	}

	void MeshServer::onChallengeResponseWrite(const boost::system::error_code& error, size_t bytesSent,
			AuthResponseMsg::ptr response)
	{
		if (!error && bytesSent != 0) {
			AuthStatusMsg::ptr authStatus(new AuthStatusMsg());

			//read the response back from the server
			boost::asio::async_read(_serviceSocket,
			boost::asio::buffer(authStatus->getDataStorage(), AuthStatusMsg::MESSAGE_SIZE),
				boost::bind(&MeshServer::onAuthenticationStatus, this,
					boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred,
					authStatus));

		} else {
			SAFELOG(AppLog::instance().error() 
				<< "[IWINTRAMESH] Unable to authenticate with asset service on intramesh server: "
				<< _assetServiceEndpoint
				<< ": "
				<< error.message()
				<< std::endl);
			
			_assetConnectionEstablished = false;
			this->onConnectionResult();
		}
	}

	void MeshServer::onRecvChallenge(const boost::system::error_code& error, size_t bytesRcvd)
	{
		if (!error && bytesRcvd != 0) {
			//generate a response to the challenge
			this->sendChallengeResponse();

		} else {
			SAFELOG(AppLog::instance().error() 
				<< "[IWINTRAMESH] Unable to authenticate with asset service on intramesh server: "
				<< _assetServiceEndpoint
				<< std::endl);
			
			_assetConnectionEstablished = false;
			this->onConnectionResult();
		}
	}

	bool MeshServer::serviceConnectionEstablished() const
	{
		return _assetConnectionEstablished;
	}

	void MeshServer::search(AssetSearchWPtr wsearch)
	{
		if (! _assetConnectionEstablished) {
			if (AssetSearch::ptr search = wsearch.lock()) {
				search->onSearchResult(shared_from_this(), search->getSearchUUID(), false);
			}

			return;
		}

		bool searchExisted = false;
		
		if (AssetSearch::ptr search = wsearch.lock()) {
			//see if we already have an outbound search for the uuid
			AssetIdToSearchMap::iterator foundList = _currentSearches.find(search->getSearchUUID());
			if (foundList == _currentSearches.end()) {
				//nothing here yet. add the set
				AssetSearchSet newSet;
				newSet.insert(wsearch);
				_currentSearches[search->getSearchUUID()] = newSet;

			} else {
				//already have at least one, add ours to the set
				foundList->second.insert(wsearch);
				searchExisted = true;
			}

			if (! searchExisted) {
				_meshQueryConnection->search(search->getSearchUUID());
			}
		}
	}

	void MeshServer::postNegativeResponse(const std::string& uuid)
	{
		this->postResponse(uuid, false);
	}

	void MeshServer::postPositiveResponse(const std::string& uuid)
	{
		this->postResponse(uuid, true);
	}

	void MeshServer::postResponse(const std::string& uuid, bool found)
	{
		//send failed so everything depending on it is screwed
		AssetIdToSearchMap::iterator foundList = _currentSearches.find(uuid);
		if (foundList != _currentSearches.end()) {
			AssetSearchSet& searchSet = foundList->second;

			BOOST_FOREACH(AssetSearch::wptr wsearch, searchSet)
			{
				if (AssetSearch::ptr search = wsearch.lock()) {
					search->onSearchResult(shared_from_this(), uuid, found);
				}
			}

			_currentSearches.erase(uuid);
		}
	}

	void MeshServer::onQueryAnswerRcvd(IntraMeshMsg::ptr inbound)
	{
		if (inbound->validate()) {
			if (inbound->getRequestType() == IntraMeshMsg::STATUS_HEARTBEAT) {
				this->updateHeartbeat(inbound->getHeartbeatFlags());

			} else if (inbound->getRequestType() == IntraMeshMsg::MESH_RESPONSE &&
				inbound->getQueryResult() == IntraMeshMsg::QR_FOUND) {

					this->postPositiveResponse(inbound->getAssetUUID());

			} else {
				this->postNegativeResponse(inbound->getAssetUUID());

			}
		}
	}

	void MeshServer::cancelSearch(AssetSearchPtr assetSearch)
	{
		AssetIdToSearchMap::iterator foundList = _currentSearches.find(assetSearch->getSearchUUID());
		if (foundList != _currentSearches.end()) {
			AssetSearchSet& searchSet = foundList->second;

			searchSet.erase(assetSearch);

			if (searchSet.empty()) {
				_currentSearches.erase(assetSearch->getSearchUUID());
			}
		}
	}

	void MeshServer::getAsset(const std::string& uuid, boost::function<void (Asset::ptr)> callBack)
	{
		if (! _assetConnectionEstablished) {
			_ioService.post(boost::bind(callBack, Asset::ptr()));
		}

		if (!_sendActive) {
			this->sendGetRequest(uuid, callBack);

		} else {
			//we already have a transfer in progress, store this transfer data for 
			_transferRequestQueue.push(PendingTransferRequest(uuid, callBack));
		}
	}

	void MeshServer::sendGetRequest(const std::string& uuid, boost::function<void (Asset::ptr)> callBack)
	{
		_sendActive = true;

		//send the request for the given asset to this server
		ClientRequestMsg::ptr request(new ClientRequestMsg(ClientRequestMsg::RT_GET, uuid));
		boost::asio::async_write(_serviceSocket,
			boost::asio::buffer(request->getHeaderData(), request->getHeaderData().size()),
			boost::bind(&MeshServer::onAfterWriteAssetRequest, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred,
					request));

		_assetCallbackQueue.push(PendingTransferRequest(uuid, callBack));
	}

	void MeshServer::onAfterWriteAssetRequest(const boost::system::error_code& error, size_t bytesSent,
		ClientRequestMsg::ptr outbound)
	{
		_sendActive = false;

		if (!error && bytesSent > 0) {
			if (_assetConnectionEstablished && !_transferRequestQueue.empty()) {
				PendingTransferRequest& req = _transferRequestQueue.front();
				this->sendGetRequest(req.get<0>(), req.get<1>());
				_transferRequestQueue.pop();
			}

			checkSafeKill();

		} else {
			SAFELOG(AppLog::instance().error() 
				<< "[IWINTRAMESH] Error while writing asset request to mesh server "
				<< _assetServiceEndpoint
				<< ": "
				<< error.message()
				<< std::endl);

			this->close();
		}
	}

	void MeshServer::listenForAssetData()
	{
		_recvActive = true;

		//read the response header
		ServerResponseMsg::ptr response(new ServerResponseMsg());
		boost::asio::async_read(_serviceSocket, 
			boost::asio::buffer(response->getHeader(), response->getHeader().size()),
			boost::bind(&MeshServer::onReadResponseHeader, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred,
				response));
	}

	void MeshServer::onReadResponseHeader(const boost::system::error_code& error, size_t bytesSent,
		ServerResponseMsg::ptr response)
	{
		_recvActive = false;

		if (!error && bytesSent > 0) {
			//is the response valid and positive?
			if (response->validateHeader() && 
				response->getResponseCode() == ServerResponseMsg::RC_FOUND) {
				
				//adjust data size
				response->initializeDataStorage();

				_recvActive = true;

#pragma warning (disable: 4503) //decorated name length exceeded
				//read the data
				boost::asio::async_read(_serviceSocket, 
					boost::asio::buffer(*(response->getData()), response->getData()->size()),
					boost::bind(&MeshServer::onReadResponseData, this,
					  boost::asio::placeholders::error,
					  boost::asio::placeholders::bytes_transferred,
					  response));

			} else {
				//error, not found, or other condition
				SAFELOG(AppLog::instance().error() 
					<< "[IWINTRAMESH] Unexpected asset error on server that already confirmed "
					<< "asset exists. "
					<< "mesh query ep: " << _meshQueryEndpoint << ", "
					<< "asset svc ep: " << _assetServiceEndpoint << ", "
					<< "valid header: " << response->validateHeader() << ", "
					<< "response code: " << (response->validateHeader() ? response->getResponseCode() : 0)
					<< std::endl);

				//invalid header disconnect this server
				if (! response->validateHeader()) {
					this->close();

				} else {
					//tell the waiter this transfer broke
					BOOST_ASSERT(! _assetCallbackQueue.empty());
					PendingTransferRequest& req = _assetCallbackQueue.front();

					if (req.get<0>() != response->getUUID()) {
						this->close();
						return;

					} else {
						req.get<1>()(Asset::ptr());
						_assetCallbackQueue.pop();
					}

					//if there is data available in the form of an error code we have to read it
					//or it'll muck up our stream positioning
					if (response->getDataSize() > 0) {
						//adjust data size
						response->initializeDataStorage();

						_recvActive = true;

						//read the data and discard
						boost::asio::async_read(_serviceSocket, 
							boost::asio::buffer(*(response->getData()), response->getData()->size()),
							boost::bind(&MeshServer::onHandleRequestErrorData, this,
							  boost::asio::placeholders::error,
							  boost::asio::placeholders::bytes_transferred,
							  response));

					} else {
						// there is no data to read. We can safely start listening again
						SAFELOG(AppLog::instance().error() 
							<< "[IWINTRAMESH] No further error data is available "
							<< std::endl);

						this->listenForAssetData();
					}
				}

				
			}

			

		} else {
			SAFELOG(AppLog::instance().error() 
				<< "[IWINTRAMESH] Error while reading asset header from mesh server "
				<< _assetServiceEndpoint
				<< ": "
				<< error.message()
				<< std::endl);

			//close the connection something is broken
			this->close();
		}
	}

	void MeshServer::onHandleRequestErrorData(const boost::system::error_code& error, size_t bytesSent,
		ServerResponseMsg::ptr response)
	{
		_recvActive = false;

		if (!error && bytesSent > 0) {
			string error;
			error.insert(error.begin(), response->getData()->begin(), response->getData()->end());

			SAFELOG(AppLog::instance().error() 
				<< "[IWINTRAMESH] Error follows: "
				<< "mesh query ep: " << _meshQueryEndpoint << ", "
				<< "asset svc ep: " << _assetServiceEndpoint << ", "
				<< "error msg: " << error
				<< std::endl);

			this->listenForAssetData();

		} else {
			this->close();
		}
	}

	void MeshServer::onReadResponseData(const boost::system::error_code& error, size_t bytesSent,
		ServerResponseMsg::ptr response)
	{
		_recvActive = false;

		if (!error && bytesSent > 0 && _assetConnectionEstablished) {
			//new asset is available
			Asset::ptr meshAsset(new Asset(response->getData()));
			
			//this asset should match the top requestor
			BOOST_ASSERT(! _assetCallbackQueue.empty());
			PendingTransferRequest& req = _assetCallbackQueue.front();

			if (meshAsset->getUUID() == req.get<0>()) {
				req.get<1>()(meshAsset);
				_assetCallbackQueue.pop();
				this->listenForAssetData();

			} else {
				//something is very wrong
				SAFELOG(AppLog::instance().error() 
					<< "[IWINTRAMESH] Asset data received does not match next waiting request "
					<< _assetServiceEndpoint
					<< " expected: " << req.get<0>()
					<< " got: " << meshAsset->getUUID()
					<< std::endl);

				//close will also signal everyone waiting
				this->close();
			}
			


		} else {
			SAFELOG(AppLog::instance().error() 
				<< "[IWINTRAMESH] Error while reading asset data from mesh server "
				<< _assetServiceEndpoint
				<< ": "
				<< error.message()
				<< std::endl);
			
			//close the connection something is broken
			this->close();
		}
	}

	void MeshServer::checkSafeKill()
	{
		if (_assetConnectionEstablished == false && _safeKillCallback) {
			if (!_recvActive && !_sendActive && !_meshQueryConnectionEstablished) {
				_safeKillCallback(shared_from_this());
			}
		}
	}

	void MeshServer::registerSafeKillCallback(SafeKillCallback safeKillCallback)
	{
		_safeKillCallback = safeKillCallback;
	}

	std::string MeshServer::getDescription()
	{
		std::stringstream journal;

		string connStatus;
		if (_assetConnectionEstablished && _meshQueryConnectionEstablished) {
			connStatus = " CONNECTED";
		} else {
			connStatus = " DEAD";
		}

		journal 
			<< this->getQueryEndpoint() 
			<< connStatus
			<< ", FLGS: " << _lastHeartbeatFlags
			<< ", Active Searches: " << _currentSearches.size()
			;
		
		return journal.str();
	}
}

