#include <sstream>
#include "Dispatcher.h"

#define TIMEOUT 2
#define USERS_PATH "users.txt"

using namespace std;
using namespace npl;

Dispatcher::Dispatcher() {
    //read users map
    serverLoader = new ServerLoader(USERS_PATH);
    this->registeredUsers = serverLoader->loadAllUserFromFile();

	running = false;
    listener = new MultipleTCPSocketListener();

}
void Dispatcher::add(TCPSocket* peer){
    if (std::find(peers.begin(),peers.end(),peer) == peers.end()) // add to vector if not exist already
	    peers.push_back(peer);
	listener->add(peer);
		if(!running){
			running = true;
			start();
		}
}
void Dispatcher::removePeer(TCPSocket* peer){
    peers.erase(std::remove(peers.begin(),peers.end(), peer),peers.end());
    listener->remove(peer);

}
void Dispatcher::run(){
    int command;
    string data;
	while(running){
			if(peers.size() == 0){
				running = false;
				break;
			}
			TCPSocket* peer = listener->listen(TIMEOUT);
			if(peer!=NULL){
				command = 0;
				data.clear();
				TCPMessengerProtocol::readFromServer(command, data, peer);
				cout<<"read command from peer: "<< command << " " << data << endl;
				switch(command){
                    case LOGIN: {
                        std::istringstream splitter(data);
                        string peerUser;
                        string peerPassword;
                        splitter >> peerUser;
                        splitter >> peerPassword;

                        bool loginSuccess = false;

                        for (map<string, string>::iterator itr = registeredUsers.begin();
                             itr != registeredUsers.end(); ++itr) {
                            if ((itr->second == peerPassword) && (itr->first == peerUser)) {

                                pair<map<string,TCPSocket*>::iterator,bool> ret = loggedInUsers.
                                        insert(pair<string, TCPSocket *>(peerUser, peer));
                                if (ret.second) {
                                    TCPMessengerProtocol::sendToServer(SUCCESS_LOGIN, peerUser + " " + peer->fromAddr(),
                                                                       peer);
                                    loginSuccess = true;
                                }
                                break;
                            }
                        }
                        if (!loginSuccess)
                            TCPMessengerProtocol::sendToServer(LOGIN_REFUSE," ", peer);
                        break;
                    }
                    case REGISTER:{
                        std::istringstream splitter1(data);
                        string peerUser1;
                        string peerPassword1;
                        splitter1 >> peerUser1;
                        splitter1 >> peerPassword1;

                        bool alreadyExist = false;

                        for (map<string,string>::iterator itr = registeredUsers.begin(); itr != registeredUsers.end() ; ++itr) {
                            if(itr->second==peerUser1){
                                TCPMessengerProtocol::sendToServer(REGISTER_REFUSE,data, peer);
                                alreadyExist = true;
                                break;
                            }
                        }
                        if (!alreadyExist){
                            //write the new user to database and check if succeeded writing
                            if (serverLoader->addNewUser(peerUser1,peerPassword1)) {
                                //insert peer to registered cache
                                registeredUsers.insert(pair<string, string>(peerUser1, peerPassword1));
                                //insert peer to logged in users cache
                                loggedInUsers.insert(pair<string, TCPSocket *>(peerUser1, peer));

                                //alert user register (and login) success
                                TCPMessengerProtocol::sendToServer(SUCCESS_REGISTER, peerUser1 + " " + peer->fromAddr(),
                                                                   peer);
                            }
                            else
                                cout << "Error writing new user to database" <<endl;
                        }
                        break;
                    }

                    case OPEN_OR_CONNECT_TO_ROOM: {
                        bool roomExists = false;
                        for (map<string, TCPSocket *>::iterator itr = loggedInUsers.begin();
                             itr != loggedInUsers.end(); ++itr){
                            if (itr->second==peer) {
                                for (int i = 0; i <chatRooms.size() ; ++i) {
                                    if (chatRooms[i]->getRoomName() == data) {
                                        TCPMessengerProtocol::sendToServer(SUCCESS_ENTER_ROOM, data, peer);
                                        this->removePeer(peer);
                                        chatRooms[i]->addUser(itr->first, peer);
                                        roomExists = true;
                                        break;
                                    }
                                }
                                if (!roomExists) {
                                    TCPMessengerProtocol::sendToServer(SUCCESS_ENTER_ROOM, data, peer);
                                    this->removePeer(peer);
                                    ChatRoom *room = new ChatRoom(this, data, itr->first, peer);
                                    chatRooms.push_back(room);
                                }
                                break;
                            }
                        }
                        break;
                    }
                    case OPEN_SESSION_WITH_PEER: {
                        if (isLoggedIn(peer)){
                            if (loggedInUsers.find(data) != loggedInUsers.end()) {
                                TCPSocket *peerB = loggedInUsers[data]; // find second peer according to the data

                                //if peer was found and client did not open session with himself then allow session
                                if (peerB != NULL && peer != peerB)
                                {
                                    //get the username from the peers
                                    string peerName, peerBName;
                                    for (map<string, TCPSocket *>::iterator itr = loggedInUsers.begin();
                                         itr != loggedInUsers.end(); ++itr) {
                                        if (itr->second == peer) {
                                            peerName = itr->first;
                                        }
                                        if (itr->second == peerB) {
                                            peerBName = itr->first;
                                        }
                                    }
                                    //tell peerB to change sessionIsActive=true
                                    TCPMessengerProtocol::sendToServer(SESSION_ESTABLISHED,
                                                                       peerBName + " " + peerB->fromAddr(), peer);
                                    TCPMessengerProtocol::sendToServer(SESSION_ESTABLISHED,
                                                                       peerName + " " + peer->fromAddr(), peerB);
                                    //remove peers from the dispatcher responsibility
                                    this->removePeer(peer);
                                    this->removePeer(peerB);
                                    //give responsibility of the peers to a new brocker
                                    Brocker *broker = new Brocker(this, peer, peerB, peerName, peerBName);
                                    //keep reference of brocker in brockers vector
                                    brockers.push_back(broker);
                                }
                                else {
                                    //get peer name
                                    string peerName;
                                    for (map<string, TCPSocket *>::iterator itr = loggedInUsers.begin();
                                         itr != loggedInUsers.end(); ++itr) {
                                        if (itr->second == peer) {
                                            peerName = itr->first;
                                        }
                                    }
                                    //if peer does not exist in peers list - refuse the session
                                    TCPMessengerProtocol::sendToServer(SESSION_REFUSED, peerName, peer);
                                    break;
                                }
                            }
                    }
                        break;
                    }
                    case LIST_USERS:{
                        string users;
                        for (map<string, string>::iterator itr = registeredUsers.begin();
                             itr != registeredUsers.end(); ++itr) {
                            users += itr->first+"\n";
                        }
                        TCPMessengerProtocol::sendToServer(LIST_USERS_RESPONSE,users,peer);
                        break;
                    }
                    case LIST_CONNECTED_USERS:{
                        string connectedUsers;
                        for (map<string, TCPSocket *>::iterator itr = loggedInUsers.begin();
                             itr != loggedInUsers.end(); ++itr) {
                            connectedUsers += itr->first+"\n";
                        }
                        TCPMessengerProtocol::sendToServer(LIST_CONNECTED_USERS_RESPONSE,connectedUsers,peer);
                        break;
                    }
                    case LIST_ROOMS:{
                        string rooms = " ";
                        for (int i = 0; i <chatRooms.size(); ++i) {
                            rooms+=chatRooms[i]->getRoomName()+"\n";
                        }
                        TCPMessengerProtocol::sendToServer(LIST_ROOMS_RESPONSE,rooms,peer);
                        break;
                    }
                    case LIST_ROOM_USERS:{
                        for (int i = 0; i <chatRooms.size() ; ++i) {
                            if(chatRooms[i]->getRoomName()==data){
                                TCPMessengerProtocol::sendToServer(LIST_ROOM_USERS_RESPONSE,chatRooms[i]->getUsers(),peer);
                                break;
                            }
                        }
                        break;
                    }
                    case EXIT: {
                        this->removePeer(peer);
                        if (isLoggedIn(peer)) {
                            for (map<string, TCPSocket *>::iterator itr = loggedInUsers.begin();
                                 itr != loggedInUsers.end(); ++itr){
                                if(itr->second==peer){
                                    loggedInUsers.erase(itr->first);
                                    break;
                                }
                            }
                        }
                        cout << "Client " << peer->fromAddr() << " has disconnected" << endl;
                        break;
                    }
                    default: {
                        cout << "Problems with client: " << peer->fromAddr() <<". disconnecting"<<endl;
                        removePeer(peer);
                        for (map<string, TCPSocket *>::iterator itr = loggedInUsers.begin();
                             itr != loggedInUsers.end(); ++itr){
                            if(itr->second==peer){
                                loggedInUsers.erase(itr->first);
                            }
                        }
                        break;
                    }
				}

			}
		}
}
void Dispatcher::close(){

}
void Dispatcher::onClose(Brocker * brocker, TCPSocket* peerA,TCPSocket* peerB){
    //remove the brocker from the brockers vector
    this->brockers.erase(std::remove(brockers.begin(),brockers.end(), brocker),brockers.end());
    //return peerA and B to the vector
    this->add(peerA);
    this->add(peerB);
    //delete the brocker
    brocker->waitForThread();

}
void Dispatcher::onClose(ChatRoom* chatRoom, map<string,TCPSocket*> peersMap){
        //remove the brocker from the brockers vector
        chatRooms.erase(std::remove(chatRooms.begin(), chatRooms.end(), chatRoom), chatRooms.end());
        loggedInUsers.erase(peersMap.begin());
        //return the peers to the vector
        for (map<string, TCPSocket *>::iterator itr = peersMap.begin(); itr != peersMap.end(); ++itr) {
            if(itr!=peersMap.begin())
                this->add(itr->second);
        }
        //delete the brocker
        chatRoom->waitForThread();
}
void Dispatcher::onClientExit(ChatRoom *chatRoom, TCPSocket * peer){
        for (map<string, TCPSocket *>::iterator itr = loggedInUsers.begin();
             itr != loggedInUsers.end(); ++itr){
            if(itr->second==peer){
                loggedInUsers.erase(itr->first);
            }
        }
        cout << "Client " << peer->fromAddr() << " has disconnected" << endl;
}
void Dispatcher::onClientExit(Brocker *brocker, TCPSocket *disconnectingPeer, TCPSocket *peerB) {
    //remove the brocker from the brockers vector
    this->brockers.erase(std::remove(brockers.begin(),brockers.end(), brocker),brockers.end());
    //return peerA and B to the vector
    this->add(peerB);

    for (map<string, TCPSocket *>::iterator itr = loggedInUsers.begin();
         itr != loggedInUsers.end(); ++itr){
        if(itr->second == disconnectingPeer){
            loggedInUsers.erase(itr);
        }
    }
    cout << "Client " << disconnectingPeer->fromAddr() << " has disconnected" << endl;
    //delete the brocker
    brocker->waitForThread();
    cout << "Closed a session"<<endl;
}
bool Dispatcher::isLoggedIn(TCPSocket *sock){
    for (map<string, TCPSocket *>::iterator itr = loggedInUsers.begin();
         itr != loggedInUsers.end(); ++itr) {
        if(itr->second == sock){
            return true;
        }
    }
    return false;
}


void Dispatcher::listUsers(){
    cout << "********Users*******" <<endl;
    for (map<string, string>::iterator itr = registeredUsers.begin();
         itr != registeredUsers.end(); ++itr) {
        cout << itr->first << endl;
    }
    cout << "*******************" <<endl;
}
void Dispatcher::listConnectedUsers(){
    cout << "********Connected Users*******" <<endl;
    for (map<string, TCPSocket*>::iterator itr = loggedInUsers.begin();
         itr != loggedInUsers.end(); ++itr) {
        cout << itr->first << endl;
    }
    cout << "*******************" <<endl;
}
void Dispatcher::listSessions(){
    cout << "********Active Sessions*******" <<endl;
    for (vector<Brocker*>::iterator itr = brockers.begin(); itr != brockers.end(); ++itr) {
        cout <<"Session: ";
        cout << "["<< (*itr)->getPeerAName() <<"] and ["<< (*itr)->getPeerBName() <<"]" << endl;
    }
    cout << "*******************" <<endl;
}
void Dispatcher::listRooms(){
    cout << "********Rooms*******" <<endl;
    for (vector<ChatRoom*>::iterator itr = this->chatRooms.begin();
         itr != chatRooms.end(); ++itr) {
        cout << (*itr)->getRoomName() << endl;
    }
    cout << "*******************" <<endl;
}
void Dispatcher::listRoomUsers(const string& roomName){
    for (vector<ChatRoom*>::iterator itr = this->chatRooms.begin();
         itr != chatRooms.end(); ++itr) {
        if ((*itr)->getRoomName() == roomName) {
            cout << "********Users in Room: [" << (*itr)->getRoomName() << "] *******" <<endl;
            cout << (*itr)->getUsers() << endl;
            cout << "*******************" <<endl;
            return;
        }
    }
    cout << "No such room: [" << roomName << "]" << endl;
}


Dispatcher::~Dispatcher() {
	// TODO Auto-generated destructor stub
}