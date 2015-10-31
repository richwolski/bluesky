#include "socket_pool.h"

SocketPool::SocketPool(int max_sockets,
               asio::io_service &io_svc
              )
:_issued(0),
 _max_sockets(max_sockets),
 _io_service(io_svc)
{
}

void SocketPool::setEndpoint(const tcp::endpoint &endpoint)
{
    _endpoint = endpoint;
}

void SocketPool::cancelAndClear()
{
    for (set<shared_ptr<tcp::socket> >::iterator i = _set.begin();
         i != _set.end();
         ++i)
    {
        (*i)->cancel();
    }

    while (!_queue.empty()) _queue.pop();
    _set.clear();
}

shared_ptr<tcp::socket> SocketPool::getSocket()
{
    mutex::scoped_lock lock(_sockets_lock);

    while (_queue.size() == 0 && _issued >= _max_sockets)
        _sockets_non_empty.wait(lock);

    if (_queue.size())
    {
            shared_ptr<tcp::socket> socket = _queue.front();
            _queue.pop();

            return socket;
    }
    else
    {
        ++_issued;
        error_code error = asio::error::host_not_found;

        shared_ptr<tcp::socket> socket(new tcp::socket(_io_service));
        socket->connect(_endpoint, error);

        if (error) throw syserr::system_error(error);

        _set.insert(socket);

        return socket;
    }
}

void SocketPool::putSocket(shared_ptr<tcp::socket> socket)
{
    mutex::scoped_lock lock(_sockets_lock);

    if (!socket->is_open())
    {
        cerr << "socket closed\n";
        --_issued;
        _set.erase(socket);
    }
    else 
    {
        _queue.push(socket);
    }

    _sockets_non_empty.notify_one();
}

SocketCheckout::SocketCheckout(SocketPool *pool)
    :_socket(pool->getSocket()),
     _pool(pool)
{
}

SocketCheckout::~SocketCheckout()
{
    _pool->putSocket(_socket);
}
tcp::socket& SocketCheckout::operator*()
{
    return *_socket;
}

tcp::socket* SocketCheckout::operator->()
{
    return _socket.get();
}

shared_ptr<tcp::socket>& SocketCheckout::socket()
{
    return _socket;
}
