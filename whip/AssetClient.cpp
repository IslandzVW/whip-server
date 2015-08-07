#include "StdAfx.h"

#include "AssetClient.h"
#include "CSUnauthenticated.h"
#include "AssetServer.h"

#include <cmath>

using boost::asio::ip::tcp;

int AssetClient::_reqSinceLastPoll = 0;
int AssetClient::_kbTransferredSinceLastPoll = 0;
int AssetClient::_meshKbTransferredSinceLastPoll = 0;

/*
 * Stats functions
 */
int AssetClient::GetNumRequestsStat()
{
	int req = _reqSinceLastPoll;
	_reqSinceLastPoll = 0;

	return req;
}

int AssetClient::GetKBTransferredStat()
{
	int transfer = _kbTransferredSinceLastPoll;
	_kbTransferredSinceLastPoll = 0;

	return transfer;
}

int AssetClient::GetMeshKBTransferredStat()
{
	int transfer = _meshKbTransferredSinceLastPoll;
	_meshKbTransferredSinceLastPoll = 0;

	return transfer;
}

void AssetClient::AddRequestToStats()
{
	++_reqSinceLastPoll;
}

void AssetClient::AddTransferToStats(int bytes)
{
	//count anything under 1k as 1k
	if (bytes < 1000) {
		_kbTransferredSinceLastPoll += 1;

	} else {
		_kbTransferredSinceLastPoll += (int) floorf((bytes / 1000.0f) + 0.5f);

	}
}

void AssetClient::AddMeshTransferToStats(int bytes)
{
	//count anything under 1k as 1k
	if (bytes < 1000) {
		_meshKbTransferredSinceLastPoll += 1;

	} else {
		_meshKbTransferredSinceLastPoll += (int) floorf((bytes / 1000.0f) + 0.5f);

	}
}


AssetClient::AssetClient(boost::asio::io_service& ioService, AssetServer& server, 
						 IAsyncStorageBackend::ptr storage, iwintramesh::MeshStorageBackend::ptr meshStorage) : 
_ioService(ioService), _assetServer(server), _conn(ioService), _storage(storage),
_meshStorage(meshStorage)
{
	
}

AssetClient::~AssetClient()
{
	SAFELOG(AppLog::instance().out() << "~AssetClient" << std::endl);
}

void AssetClient::setInitialState()
{
	IConnectionState::ptr initialState(new CSUnauthenticated(_ioService, _storage, _meshStorage));
	this->setState(initialState);
}

tcp::socket& AssetClient::getConn()
{
	return _conn;
}

void AssetClient::setState(IConnectionState::ptr state)
{
	_state = state;
	_state->setClient(shared_from_this());
}

void AssetClient::startState()
{
	_state->start();
}

void AssetClient::start()
{
	try {
		SAFELOG(AppLog::instance().out() << "New client connection: " << _conn.remote_endpoint().address().to_string() << std::endl);
		_ep = _conn.remote_endpoint();
	} 
	catch (...) {
		//just here because _conn.remote_endpoint() can throw if the socket is now somehow disconnected
	}

	_assetServer.clientConnect(shared_from_this());
	this->startState();
}

void AssetClient::disconnect()
{
	SAFELOG(AppLog::instance().out() << "Client disconnect: " << _ep.address().to_string() << std::endl);
	_conn.close();
	_assetServer.clientDisconnect(shared_from_this());
}

AssetServer& AssetClient::getParentServer()
{
	return _assetServer;
}

std::string AssetClient::getStatus()
{
	try {
		return _ep.address().to_string() + ": " + _state->getStateDescription();
	} catch (...) {
		return "DISCONNECTED";
	}
}