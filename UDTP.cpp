#include "UDTP.h"
/*Packets, Implementation after forward declaration*/
#include "UDTPSetup.h"
#include "UDTPPacket.h"
#include "UDTPPath.h"
#include "UDTPFile.h"
#include "UDTPHeader.h"
#include "UDTPChunk.h"
#include "UDTPAcknowledge.h"
#include "UDTPHandshake.h"
#include "UDTPAddress.h"
#include "UDTPPeer.h"
#include "UDTPThreadFlow.h"
#include "UDTPThreadFile.h"
#include "UDTPThreadProcess.h"
#include <fstream>
#include <string.h>
#define EMPTY 0x00

SocketReturn UDTP::start(SocketType socketType)
{
    _uniqueIDCount = 0;
    _flowThreadCount = 0;
    if(_isAlive) return ALREADY_RUNNING;
    _socketType = socketType;

    /*TCP set up!*/
    if (!start_listen_socket(socketType)) return SOCKET_NOT_INIT; /*starts a listen socket on bind type*/
    /*Flow socket setup!*/
    /*Important part*/
    if(!start_mutex()) return COULD_NOT_START_MUTEX;

    socketType == HOST ? display_msg("HOST has successfully completed socket setup.") : display_msg("HOST has successfully completed socket setup.");
    unsigned int peerID = add_peer(_listenSocket); /*Add self as socket. Doesn't matter if you're server or client!*/
    self_peer()->set_unique_id(0); /*self peer has unique_id of zero*/
    socketType == HOST ? self_peer()->set_host_local(true): self_peer()->set_host_local(false);
    self_peer()->set_init_process_complete(MUTEX_FRAMEWORK_INIT);
    self_peer()->set_init_process_complete(LISTEN_SOCKET);

    if(!start_queue_threads()) return COULD_NOT_START_THREADS;
    if(!start_listen_thread()) return COULD_NOT_START_THREADS;

    _isAlive = true;
    return SUCCESS;

}
bool UDTP::start_listen_socket(SocketType bindType)
{
    _listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); /*TCP Socket!*/
    if(_listenSocket < 0) return false;
    struct sockaddr_in listenAddress; /*TCP Struct address!*/
    memset(&listenAddress, 0, sizeof(listenAddress));
    listenAddress.sin_port = htons(setup()->get_port());
    listenAddress.sin_family = AF_INET;


    switch (bindType)
    {
    case HOST:
        listenAddress.sin_addr.s_addr = INADDR_ANY;
        if(bind(_listenSocket, (struct sockaddr*)&listenAddress, sizeof(struct sockaddr_in)) < 0) return false;
        if(listen(_listenSocket,0) < 0) return false;
        break;
    case PEER:
        listenAddress.sin_addr.s_addr = inet_addr(setup()->get_ip());
        if(connect(_listenSocket, (struct sockaddr*)&listenAddress, sizeof(struct sockaddr_in)) < 0) return false;
        break;
    }

        unsigned int optval = 1;
        socklen_t optlen = sizeof(optval);
        if(setsockopt(_listenSocket, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0){
            perror("setsockopt on HOST");
            return SOCKET_NOT_INIT;
        }
    return true;
}

bool UDTP::start_mutex()
{
    display_msg("Mutex have been started");
    if (pthread_mutex_init(&_mutexFlowThread, NULL) != 0) return false;
    if (pthread_mutex_init(&_mutexPeer, NULL) != 0) return false;
    if(sem_init(&_semListenPacketQueue,0,0) != 0) return false;


    return true;
}

bool UDTP::stop()
{
    _isAlive = false;

}

unsigned int UDTP::add_peer(unsigned int listenSocketOfPeer)
{
    unsigned int posID = 0;
    pthread_mutex_lock(&_mutexPeer); /*Adds a lock just in case*/
    UDTPPeer* newPeer = new UDTPPeer((UDTP*)this, listenSocketOfPeer);
    newPeer->set_online();
    _listPeers.push_back(newPeer);
    posID = _listPeers.size() - 1; /*Capacity minus 1!*/
    pthread_mutex_unlock(&_mutexPeer);
    return posID;
}
int UDTP::find_peer_pos(unsigned int listenSocketOfPeer)
{
    pthread_mutex_lock(&_mutexPeer);
    for(unsigned int posID=0; posID<_listPeers.size(); posID++)
    {
        if(_listPeers[posID]->get_listen_socket() == listenSocketOfPeer)
        {
            pthread_mutex_unlock(&_mutexPeer);
            return posID;
        }
    }
    return 0; /*This return 0, since self is ZERO, this means false!*/
    pthread_mutex_unlock(&_mutexPeer);
}
int UDTP::find_peer_using_address(sockaddr_in searchAddress)
{
    pthread_mutex_lock(&_mutexPeer);
    for(int posID=0; posID<_listPeers.size(); posID++)
    {
        if((inet_ntoa(searchAddress.sin_addr)) ==inet_ntoa(_listPeers[posID]->get_address().sin_addr))
        {
            if((ntohs(searchAddress.sin_port)) ==ntohs(_listPeers[posID]->get_address().sin_port))
            {
                pthread_mutex_unlock(&_mutexPeer);
                return posID;
            }
        }
    }
    pthread_mutex_unlock(&_mutexPeer);
    return -1; /*Nothing was found!*/

}
UDTPPeer* UDTP::get_peer(unsigned int posID)
{
    pthread_mutex_lock(&_mutexPeer);
    unsigned int sizeOfListPeers = _listPeers.size();
    pthread_mutex_unlock(&_mutexPeer);
    if(!(posID >= 0 && posID < sizeOfListPeers)) return NULL; /*Out of bounds!*/
    return _listPeers[posID];
}

bool UDTP::remove_peer(unsigned int posID)
{
    pthread_mutex_lock(&_mutexPeer);
    if(!(posID >= 0 && posID < _listPeers.size()))
    {
        pthread_mutex_unlock(&_mutexPeer);
        return false; /*Out of bounds!*/
    }

    delete _listPeers[posID];
    _listPeers.erase(_listPeers.begin()+posID);
    pthread_mutex_unlock(&_mutexPeer);
    return true;
}
  bool UDTP::approve_pending_file(UDTPHeader* compare){
  }
void UDTP::add_queue_listen(UDTPPacket* packet){
    display_msg("Packet was added to request queue!");
    _listenPacketQueue.push(packet);
}

void* UDTP::listenQueueThread(void* args){ /*This will handle all listen packets*/
    UDTP *accessUDTP = (UDTP*) args;
    while(accessUDTP->alive()){
        sem_wait(&accessUDTP->_semListenPacketQueue);
        while(!accessUDTP->_listenPacketQueue.empty()){

        }

    }
    accessUDTP = NULL;
}
void* UDTP::listenThread(void* args)
{

    bool sentRequiredPackets = false; /*For client*/

    UDTP *accessUDTP = (UDTP*) args;
    std::vector<pollfd> activeListenSockets;
    pollfd masterPollFd;
    masterPollFd.fd = accessUDTP->_listenSocket;
    masterPollFd.events = POLLIN;
    activeListenSockets.push_back(masterPollFd);

    pollfd* activeListenSocketsPtr;
    int activeListenActivity;

    std::queue<unsigned int> removePeerAndSocketSafely;
    if(accessUDTP->get_socket_type() == PEER) accessUDTP->display_msg("PEER's polling has been configured and is starting");
    if(accessUDTP->get_socket_type() == HOST) accessUDTP->display_msg("HOST's polling has been configured and is starting");
    while(accessUDTP->alive())
    {
        /*********Beginning of Server Code**********/
        if(accessUDTP->_socketType == HOST)
        {
            activeListenSocketsPtr = &activeListenSockets[0];
            activeListenActivity = poll(activeListenSocketsPtr, activeListenSockets.size(), -1);
            if(activeListenActivity < 0 ) perror("poll_host");
            if((activeListenSockets[0].revents & POLLIN))
            {
                struct sockaddr_in newPeerAddress;
                int newPeerAddressLen = sizeof(newPeerAddress);
                unsigned int newPeerListenSocket;
                if((newPeerListenSocket = accept(activeListenSockets[0].fd, (struct sockaddr*)&newPeerAddress, (socklen_t*)&newPeerAddressLen)) <0)
                {
                    perror("accept");
                }
                else
                {
                    accessUDTP->display_msg("HOST has detected a new peer and is accepting the connection");
                    pollfd newPollFd; //*Add to polling*/
                    newPollFd.fd = newPeerListenSocket;
                    newPollFd.events = POLLIN;
                    activeListenSockets.push_back(newPollFd);
                    /*Add to peers list. Both Pollfd and _listPeer are Parallel..*/
                    accessUDTP->_uniqueIDCount++;
                    unsigned int newPeerID = accessUDTP->add_peer(newPeerListenSocket);
                    accessUDTP->get_peer(newPeerID)->set_address(newPeerAddress);
                    accessUDTP->get_peer(newPeerID)->set_unique_id(accessUDTP->_uniqueIDCount);
                    accessUDTP->get_peer(newPeerID)->set_host_local(true);
                    UDTPHandshake handshakeStart(ResponseStart); /*Send this handshake request to client so he will start activate the function: send_required_packets();*/
                    handshakeStart.set_socket_id(newPeerListenSocket); /*Take socket id! so the send_listen_data function will know where to send it to!*/
                    handshakeStart.set_peer_id(newPeerID);
                    handshakeStart.set_unique_id(accessUDTP->_uniqueIDCount);
                    // TODO: Might want to refactor this code
                    accessUDTP->display_msg("HOST has sent out a HandshakeStart to notify new peer to use function");

                    handshakeStart.pack();
                    accessUDTP->get_peer(newPeerID)->send_to(&handshakeStart); /*Send out Handshake start so the client knows to start the function send_required_packets()*/
                }
            }
            for(unsigned int i=1; i<activeListenSockets.size(); i++)
            {
                if(activeListenSockets[i].revents & POLLIN)
                {
                    UDTPPacketHeader packetDeduction;
                    if((read(activeListenSockets[i].fd, &packetDeduction, sizeof(UDTPPacketHeader))) != 0)
                    {
                        if(accessUDTP->get_peer(i)->is_online()){
                        accessUDTP->display_msg("HOST has received incoming packet");
                        UDTPPacket *incomingData = 0;

                        /* Determine packet type and create new packet object */
                        accessUDTP->display_msg("HOST is processing packet deduction");
                        bool deductionValid = true;
                        /*USE packet type and build correctly. here*/
                        switch(packetDeduction.packetType)
                        {
                            // TODO: abstract out the 3 calls in each case (set_socket_id, set_peer_id, recv)
                        case Header:
                            accessUDTP->display_msg("HOST has deduced the packet as a Header and is now processing");
                            incomingData = new UDTPHeader(packetDeduction);
                            break;
                        case Path:
                            accessUDTP->display_msg("HOST has deduced the packet as a Path and is now processing");
                            incomingData = new UDTPPath(packetDeduction);
                            break;
                        case Acknowledge:
                            accessUDTP->display_msg("HOST has deduced the packet as an Acknowledge and is now processing");
                            incomingData = new UDTPAcknowledge(packetDeduction);
                            break;
                        case Handshake:
                            accessUDTP->display_msg("HOST has deduced the packet as a Handshake and is now processing");
                            incomingData = new UDTPHandshake(packetDeduction);
                            break;
                        default:
                            accessUDTP->display_msg("HOST was unable to deduce the packet type.");
                            deductionValid = false;
                            break;
                        }
                        if(deductionValid){
                            incomingData->set_socket_id(activeListenSockets[i].fd);
                            incomingData->set_peer_id(i);
                            incomingData->set_peer(accessUDTP->get_peer(i));
                            incomingData->set_unique_id(accessUDTP->get_peer(i)->get_unique_id()); /*Set unique ID!*/
                            incomingData->set_udtp(accessUDTP); /*Pass on UDTP pointer*/
                            recv(accessUDTP->_listenSocket, (char*)incomingData->write_to_buffer(), incomingData->get_packet_size(), 0);
                            /*Verifying sending check!*/
                            unsigned int resendCheck = 0x00;
                            if(incomingData->unpack()) resendCheck |= (0x01 << 0x00); /*Unpack to member variables*/
                            if(incomingData->respond()) resendCheck |= (0x01 << 0x01); /*Respond to it and apply some changes or whatever*/
                            if(incomingData->pack()) resendCheck |= (0x01 << 0x02); /*Pack it up again*/

                            if(resendCheck &  0x07){
                                incomingData->peer()->send_to(incomingData);
                            }

                            //sem_post(&accessUDTP->_semListenPacketQueue);
                        }
                        accessUDTP->display_msg("HOST has completed packet processing");
                        }
                    }
                    else
                    {
                        /*Client has disconnected*/
                        accessUDTP->get_peer(i)->set_offline();
                        accessUDTP->display_msg("HOST has detected PEER disconnect and queue them up to be removed.");
                        removePeerAndSocketSafely.push(i);
                    }
                }
            }
            while(!removePeerAndSocketSafely.empty())
            {
                unsigned int posID = removePeerAndSocketSafely.front(); /*Get pos Id, this is just a locational id in the parallel array..*/
                accessUDTP->display_msg("HOST has removed a PEER from polling and list of peers");
                activeListenSockets.erase(activeListenSockets.begin()+posID); /*Remove from polling(*/
                accessUDTP->remove_peer(posID);
                removePeerAndSocketSafely.pop();
            }
        } /*End of server code*/
        /***********End of Server Code*************/

        activeListenSocketsPtr = &activeListenSockets[0]; /*This line fixes poll: Bad Address*/
        /*********Beginning of Client Code**********/
        if(accessUDTP->_socketType == PEER)
        {

            activeListenActivity = poll(activeListenSocketsPtr, activeListenSockets.size(), -1);
            if(activeListenActivity < 0 ) perror("poll_peer");

            if((activeListenSockets[0].revents & POLLIN))
            {
                           UDTPPacketHeader packetDeduction;
                    if((read(accessUDTP->_listenSocket, &packetDeduction, sizeof(UDTPPacketHeader))) != 0)
                    {
                        accessUDTP->display_msg("PEER has received incoming packet");
                        UDTPPacket *incomingData = 0;

                        /* Determine packet type and create new packet object */
                        accessUDTP->display_msg("PEER is processing packet deduction");
                        bool deductionValid = true;
                        /*USE packet type and build correctly. here*/
                        switch(packetDeduction.packetType)
                        {
                            // TODO: abstract out the 3 calls in each case (set_socket_id, set_peer_id, recv)
                        case Header:
                            accessUDTP->display_msg("PEER has deduced the packet as a Header and is now processing");
                            incomingData = new UDTPHeader(packetDeduction);
                            break;
                        case Path:
                            accessUDTP->display_msg("PEER has deduced the packet as a Path and is now processing");
                            incomingData = new UDTPPath(packetDeduction);
                            break;
                        case Acknowledge:
                            accessUDTP->display_msg("PEER has deduced the packet as an Acknowledge and is now processing");
                            incomingData = new UDTPAcknowledge(packetDeduction);
                            break;
                        case Handshake:
                            accessUDTP->display_msg("PEER has deduced the packet as a Handshake and is now processing");
                            incomingData = new UDTPHandshake(packetDeduction);
                            break;
                        default:
                            accessUDTP->display_msg("PEER was unable to deduce the packet type.");
                            deductionValid = false;
                            break;
                        }
                        if(deductionValid){
                            incomingData->set_socket_id(activeListenSockets[0].fd);
                            incomingData->set_peer(accessUDTP->get_peer(0));
                            incomingData->set_udtp(accessUDTP); /*Pass on UDTP pointer*/
                            recv(accessUDTP->_listenSocket, (char*)incomingData->write_to_buffer(), incomingData->get_packet_size(), 0);
                            /*Verifying sending check!*/
                            unsigned int resendCheck = 0x00;
                            if(incomingData->unpack()) resendCheck |= (0x01 << 0x00); /*Unpack to member variables*/
                            if(incomingData->respond()) resendCheck |= (0x01 << 0x01); /*Respond to it and apply some changes or whatever*/
                            if(incomingData->pack()) resendCheck |= (0x01 << 0x02); /*Pack it up again*/

                            if(resendCheck &  0x07){
                                incomingData->peer()->send_from(incomingData); /*Sends from socket!*/
                            }
                        }
                        accessUDTP->display_msg("PEER has completed packet processing");
                }
                else
                {
                    accessUDTP->stop(); /*Client has disconnected or server has disconnected client.*/
                }
            }
        }
        /***********End of Client Code*************/
    }

    accessUDTP = NULL;
}
/*void UDTP::send_flow_data(UDTPData& data){ /*Only chunks really!
    while( (send(_flowSocket, data.get_raw_buffer(), data.get_packet_size(), 0)) != data.get_packet_size()); /*Keep sending until it's all sent
}*/




void UDTP::display_msg(std::string message)
{

    if(setup()->get_debug_enabled()) std::cout << message << std::endl;
}


bool UDTP::start_queue_threads(){
    pthread_create(&_listenPacketQueueThreadHandler, NULL, &UDTP::listenQueueThread, (UDTP*) this);
    pthread_tryjoin_np(_listenPacketQueueThreadHandler, NULL);


    self_peer()->set_init_process_complete(QUEUE_THREADS);

    return true;
}
bool UDTP::start_listen_thread()
{

    pthread_create(&_listenThreadHandler, NULL, &UDTP::listenThread, (UDTP*)this);
    pthread_tryjoin_np(_listenThreadHandler, NULL);
    self_peer()->set_init_process_complete(LISTEN_THREAD);

    return true;
}

SocketType UDTP::get_socket_type()
{
    return _socketType;
}
bool UDTP::alive()
{

    return _isAlive;
}
    UDTPFile* UDTP::get_file_with_id(unsigned int fileID){
    for(unsigned int i=0; i<_overallFiles.size();i++){
        if(_overallFiles[i]->get_file_id() == fileID){
            return _overallFiles[i];
        }
    }
    return NULL;
}
bool UDTP::add_file_to_list(UDTPFile* file)
{ /*This adds a file to the log! No deleting or nothing!*/
        _overallFiles.push_back(file);
}


/***************************PEER ONLY COMMANDS*************************************/
TransferReturn UDTP::send_file(std::string path){
    if(!self_peer()->check_init_process(COMPLETE)) return SELF_NOT_READY;
    UDTPFile* sendFile = new UDTPFile(path);
    if(!sendFile->check_file_exist()) return FILE_NOT_EXIST;
    sendFile->retrieve_info_from_local_file();

    if(get_socket_type() == HOST) sendFile->set_file_id(get_next_file_id());
    if(get_socket_type() == PEER) sendFile->set_file_id(0); /*PEERS always send a zero!*/

    UDTPHeader* requestFileSend = new UDTPHeader;
    sendFile->pack_to_header(*requestFileSend); /*Packs it in!*/


    requestFileSend->pack();
    self_peer()->send_from(requestFileSend);
    delete requestFileSend;
}

TransferReturn UDTP::get_file(std::string path){
    if(!self_peer()->check_init_process(COMPLETE)) return SELF_NOT_READY;
    UDTPFile* getFile  = new UDTPFile(path);
    if(getFile->check_file_exist()) return FILE_ALREADY_EXIST;
    getFile->set_info_to_zero();

    if(get_socket_type() == HOST) getFile->set_file_id(get_next_file_id());
    if(get_socket_type() == PEER) getFile->set_file_id(0); /*PEERS always send a zero!*/

    UDTPHeader* requestFileGet = new UDTPHeader;
    getFile->pack_to_header(*requestFileGet); /*Packs it in!*/

    requestFileGet->pack();
    self_peer()->send_from(requestFileGet);
    delete requestFileGet;
}
