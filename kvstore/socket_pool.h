#ifndef _SOCKET_POOL_H_
#define _SOCKET_POOL_H_ 1

#include <iostream>
#include <set>
#include <queue>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include "util.h"

using namespace std;
using namespace boost;


class SocketPool
{
public:
    SocketPool(int max_streams,
               asio::io_service &io_svc);
    void setEndpoint(const tcp::endpoint &endpoint);
    void cancelAndClear();
    shared_ptr<tcp::socket> getSocket();
    void putSocket(shared_ptr<tcp::socket> socket);
private:
    int _issued;
    int _max_sockets;
    mutex _sockets_lock;
    condition_variable _sockets_non_empty;
    asio::io_service &_io_service;
    tcp::endpoint _endpoint;
    queue<shared_ptr<tcp::socket> > _queue;
    set<shared_ptr<tcp::socket> > _set;
};

class SocketCheckout
{
public:
    SocketCheckout(SocketPool *pool);
    ~SocketCheckout();

    tcp::socket& operator*();
    tcp::socket* operator->();

    shared_ptr<tcp::socket>& socket();

private:
    shared_ptr<tcp::socket> _socket;
    SocketPool *_pool;
};

#endif
