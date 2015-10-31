#ifndef __PROTOBUFRPC_H__
#define __PROTOBUFRPC_H__

#include <stdint.h>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <queue>
#include <set>
#include "socket_pool.h"
#include "util.h"

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/service.h>

using namespace std;
using namespace google::protobuf;
using namespace boost;

namespace bicker
{

class ProtoBufRpcService;

class ProtoBufRpcConnection 
    : public enable_shared_from_this<ProtoBufRpcConnection>,
      private boost::noncopyable
{
public:
    explicit ProtoBufRpcConnection(asio::io_service& io_service,
                                 Service *_service);

    asio::ip::tcp::socket& socket();

    void start();

    void writeResponse(Message *msg);


private:
    void handle_read(const error_code& e, 
                     std::size_t bytes_transferred);

    void handle_write(const error_code& e,
                     std::size_t bytes_transferred);

    tcp::socket _socket;


    asio::io_service::strand _strand;

    Service *_service;

    asio::streambuf _buffer;

    int _id;
    unsigned int _len;

    enum {
        STATE_NONE,
        STATE_HAVE_ID_AND_LEN,
        STATE_WAITING_FOR_DATA,
        STATE_FAIL,
    } _state;
};


class ProtoBufRpcServer
{
public:
    ProtoBufRpcServer();

    bool registerService(uint16_t port,
                         shared_ptr< Service> service);

    // So we can call this as a thread.
    void run();

    // So we can stop..
    void shutdown();

protected:

    class RegisteredService
    {
    public:
        RegisteredService(
                          shared_ptr<asio::io_service> io_service,
                          uint16_t port,
                          shared_ptr<Service> service
                         );

        void handle_accept(const error_code& e);

    protected:
        // Ref to parent's
        shared_ptr<asio::io_service> _io_service;
        uint16_t _port;
        shared_ptr<Service> _service;
        tcp::endpoint _endpoint;
        tcp::acceptor _acceptor;
        shared_ptr<ProtoBufRpcConnection> _new_connection;
    };

    list<shared_ptr<RegisteredService> > _services;
    shared_ptr<asio::io_service> _io_service;
};

class ProtoBufRpcController : public RpcController
{
public:
    ProtoBufRpcController();
    virtual ~ProtoBufRpcController();

    virtual void Reset();
    virtual bool Failed() const;
    virtual string ErrorText() const;
    virtual void StartCancel();

    virtual void SetFailed(const string &reason);
    virtual bool IsCanceled() const;
    virtual void NotifyOnCancel(Closure *callback);
};

class ProtoBufRpcChannel
    : public RpcChannel,
      public enable_shared_from_this<ProtoBufRpcChannel>,
      private boost::noncopyable
{
public:
    ProtoBufRpcChannel(const string &remotehost, const string &port);

    virtual ~ProtoBufRpcChannel();

    virtual void CallMethod(
        const MethodDescriptor * method,
        RpcController * controller,
        const Message * request,
        Message * response,
        Closure * done);

protected:
    shared_ptr<tcp::socket> getSocket();
    void putSocket(shared_ptr<tcp::socket>);

private:
    class MethodHandler;

    string _remote_host;
    string _port;


    asio::io_service _io_service;
    tcp::resolver _resolver;
    // This exists to keep the io service running
    tcp::acceptor _acceptor;

    SocketPool _pool;

    asio::ip::tcp::socket _lame_socket;
    void lame_handle_accept(const error_code &err);

    shared_ptr<boost::thread> _thread;

};

} // namespace bicker

#endif
