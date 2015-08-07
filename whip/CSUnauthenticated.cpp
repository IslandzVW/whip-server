#include "StdAfx.h"

#include <boost/bind.hpp>

#include "CSUnauthenticated.h"
#include "CSAuthenticated.h"
#include "AuthChallengeMsg.h"
#include "AssetClient.h"

CSUnauthenticated::CSUnauthenticated(boost::asio::io_service& ioService, IAsyncStorageBackend::ptr storage,
									 iwintramesh::MeshStorageBackend::ptr meshStorage)
: _ioService(ioService), _storage(storage), _meshStorage(meshStorage), _isMeshConnection(false)
{

}

CSUnauthenticated::~CSUnauthenticated()
{
}

void CSUnauthenticated::setClient(AssetClientPtr client)
{
	_client = client;
}

void CSUnauthenticated::sendAuthChallenge()
{
	_challenge.reset(new AuthChallengeMsg(false));
	const whip::byte_array& challengeData = _challenge->serialize();
	
	AssetClient::ptr client;
	if (client = _client.lock()) {
		boost::asio::async_write(client->getConn(),
			boost::asio::buffer(challengeData, challengeData.size()),
			boost::bind(&CSUnauthenticated::handleWriteChallenge, shared_from_this(),
				  boost::asio::placeholders::error));
	}
}

void CSUnauthenticated::start()
{
	//send off the auth challenge
	this->sendAuthChallenge();
}

void CSUnauthenticated::handleWriteChallenge(const boost::system::error_code& error)
{
	if (! error) {
		//challenge write succeeded, let's get ready for the response
		AssetClient::ptr client;
		if (client = _client.lock()) {
			_response.reset(new AuthResponseMsg());

			boost::asio::async_read(client->getConn(),
				boost::asio::buffer(_response->getDataStorage(), AuthResponseMsg::MESSAGE_SIZE),
				boost::bind(
				  &CSUnauthenticated::handleReadChallengeResponse, shared_from_this(),
					boost::asio::placeholders::error));

		} else {
			SAFELOG(AppLog::instance().out() << "handleWriteChallenge():  Client instance disappeared" << std::endl);
		}

	} else {
		//challenge write failed, so remove this client
		AssetClient::ptr client;
		if (client = _client.lock()) {
			client->disconnect();
			SAFELOG(AppLog::instance().out() << "handleWriteChallenge(): Connection failed " << error.message() << std::endl);
		} else {
			SAFELOG(AppLog::instance().out() << "handleWriteChallenge(): Client instance disappeared" << std::endl);
		}
	}
}

void CSUnauthenticated::handleReadChallengeResponse(const boost::system::error_code& error)
{
	if (! error) {
		//test if the auth succeeded
		if (_response->isValid(_challenge)) {
			//write the successful auth status message
			_authStatus.reset(new AuthStatusMsg(AS_AUTH_SUCCESS));
			
			AssetClient::ptr client;
			if (client = _client.lock()) {
				
				int BUF_SZ = whip::Settings::instance().get("tcp_bufsz").as<int>();

				client->getConn().set_option(boost::asio::socket_base::send_buffer_size(BUF_SZ));
				client->getConn().set_option(boost::asio::socket_base::receive_buffer_size(BUF_SZ));

				if (_response->isServerResponse()) {
					_isMeshConnection = true;
				}

				boost::asio::async_write(client->getConn(),
					boost::asio::buffer(_authStatus->serialize(), _authStatus->serialize().size()),
					boost::bind(&CSUnauthenticated::handleWriteAuthSuccess, shared_from_this(),
						  boost::asio::placeholders::error));
			}
		} else {
			//write the failure auth message
			SAFELOG(AppLog::instance().out() << "Invalid auth challenge response, disconnecting" << std::endl);
			
			_authStatus.reset(new AuthStatusMsg(AS_AUTH_FAILURE));
			
			AssetClient::ptr client;
			if (client = _client.lock()) {
				boost::asio::async_write(client->getConn(),
					boost::asio::buffer(_authStatus->serialize(), _authStatus->serialize().size()),
					boost::bind(&CSUnauthenticated::handleWriteAuthFailure, shared_from_this(),
						  boost::asio::placeholders::error));
			}
		}

	} else {
		//challenge write failed, so remove this client
		AssetClient::ptr client;
		if (client = _client.lock()) {
			client->disconnect();
			SAFELOG(AppLog::instance().out() << "handleReadChallengeResponse(): Connection failed " << error.message() << std::endl);
		} else {
			SAFELOG(AppLog::instance().out() << "handleReadChallengeResponse(): Client instance disappeared" << std::endl);
		}

	}
}

void CSUnauthenticated::handleWriteAuthFailure(const boost::system::error_code& error)
{
	AssetClient::ptr client;
	if (client = _client.lock()) {
		client->disconnect();
	}
}

void CSUnauthenticated::handleWriteAuthSuccess(const boost::system::error_code& error)
{
	//change client state to authenticated
	AssetClient::ptr client;
	if (client = _client.lock()) {
		IConnectionState::ptr authState(new CSAuthenticated(_ioService, _storage, _meshStorage, _isMeshConnection));
		client->setState(authState);
		client->startState();
	}
}

std::string CSUnauthenticated::getStateDescription()
{
	return "Unauthenticated";
}