/*
 * Dispatcher.h
 *
 *  Created on: May 8, 2016
 *      Author: parallels
 */

#ifndef SRC_DISPATCHER_H_
#define SRC_DISPATCHER_H_

#include <algorithm>
#include <vector>
#include "Brocker.h"
#include "../../SocketUtils/src/MultipleTCPSocketListener.h"
#include "../../SocketUtils/src/TCPMessengerProtocol.h"

namespace npl {

class Dispatcher : public MThread, Brocker::Handler {
	vector<TCPSocket*> peers;
	vector<Brocker*> brockers;
	MultipleTCPSocketListener* listener;
	bool running;
public:
	Dispatcher();
	void add(TCPSocket* peer);
	TCPSocket* getPeerByAddress(const string& address);
	void removePeer(TCPSocket* peer);
	void listPeers();
	void run();
	void close();
	virtual ~Dispatcher();

	virtual void onClose(Brocker * brocker, TCPSocket* peerA,TCPSocket* peerB);

};


} /* namespace npl */

#endif /* SRC_DISPATCHER_H_ */