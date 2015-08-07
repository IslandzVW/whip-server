#pragma once

#include <boost/shared_ptr.hpp>

class AssetClient;
typedef boost::shared_ptr<AssetClient> AssetClientPtr;

/**
 * Interface for objects that will handle connection state for AssetClients
 */
class IConnectionState
{
public:
	virtual ~IConnectionState();

	/**
	 * Pointer type
	 */
	typedef boost::shared_ptr<IConnectionState> ptr;


	/**
	 * Tells the state it's parent client
	 */
	virtual void setClient(AssetClientPtr client) = 0;

	/**
	 * Tells the state to begin processing packets
	 */
	virtual void start() = 0;

	/**
	 * Returns a textual description of the current state of the connection
	 */
	virtual std::string getStateDescription() = 0;
};
