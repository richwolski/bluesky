#include "protobufrpc.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>

#include <boost/functional/hash.hpp>

using namespace std;

using namespace boost;
using asio::buffer;

namespace bicker
{

template<typename T>
static void* void_write(void* data, T val)
{
    *((T*)data) = val;
    return (char*)data + sizeof(T);
}

class ProtoBufRpcServiceRequest
{
public:
    ProtoBufRpcServiceRequest(
                            RpcController *ctrl,
                            const MethodDescriptor* method,
                            Message *request,
                            Message *response,
                            shared_ptr<ProtoBufRpcConnection> conn
                           )
        :_ctrl(ctrl),
         _method(method),
         _request(request),
         _response(response),
         _conn(conn)
    {
    }

    ~ProtoBufRpcServiceRequest()
    {

    }

    static void run(ProtoBufRpcServiceRequest *req)
    {

        req->_conn->writeResponse(req->_response.get());

        delete req;
    }

    shared_ptr<RpcController> _ctrl;
    const MethodDescriptor *_method;
    shared_ptr<Message> _request;
    shared_ptr<Message> _response;
    shared_ptr<ProtoBufRpcConnection> _conn;
};

ProtoBufRpcConnection::ProtoBufRpcConnection(asio::io_service& io_service,
                             Service *service)
:_socket(io_service),
 _strand(io_service),
 _service(service),
 _state(STATE_NONE)
{
}

tcp::socket& ProtoBufRpcConnection::socket()
{
    return _socket;
}

void ProtoBufRpcConnection::start()
{
    _socket.async_read_some(_buffer.prepare(4096),
            _strand.wrap(
                boost::bind(&ProtoBufRpcConnection::handle_read, shared_from_this(),
                            asio::placeholders::error,
                            asio::placeholders::bytes_transferred)));
}

void ProtoBufRpcConnection::writeResponse(Message *msg)
{
    int rlen = msg->ByteSize();
    int len = htonl(rlen);
    int mlen = sizeof(len) + rlen;

    void * data = asio::buffer_cast<void*>(_buffer.prepare(mlen));

    data = void_write(data, len);

    using google::protobuf::io::ArrayOutputStream;

    ArrayOutputStream as(data, rlen);

    msg->SerializeToZeroCopyStream(&as);

    _buffer.commit(mlen);

    asio::async_write(_socket,
            _buffer.data(),
            _strand.wrap(
                boost::bind(&ProtoBufRpcConnection::handle_write, 
                            shared_from_this(),
                asio::placeholders::error,
                asio::placeholders::bytes_transferred)));
}


void ProtoBufRpcConnection::handle_read(const error_code& e, 
                 std::size_t bytes_transferred)
{
    if (!e)
    {
        _buffer.commit(bytes_transferred);

        if (_state == STATE_NONE)
        {
            if (_buffer.size() >= sizeof(_id) + sizeof(_len))
            {
                string b(
                     buffers_begin(_buffer.data()),
                     buffers_begin(_buffer.data())
                                                + sizeof(_id) + sizeof(_len)
                        );

                _buffer.consume(sizeof(_id) + sizeof(_len));

                _id = *((int*)b.c_str());
                _id = ntohl(_id);

                _len = *((unsigned int*)(b.c_str() + sizeof(_id)));
                _len = ntohl(_len);

                _state = STATE_HAVE_ID_AND_LEN;
            }
            else
            {
                start();
            }
        }

        if (_state == STATE_HAVE_ID_AND_LEN || _state == STATE_WAITING_FOR_DATA)
        {
            if (_buffer.size() >= _len)
            {
                const MethodDescriptor* method =
                    _service->GetDescriptor()->method(_id);

                Message *req = _service->GetRequestPrototype(method).New();
                Message *resp = _service->GetResponsePrototype(method).New();

                using google::protobuf::io::ArrayInputStream;
                using google::protobuf::io::CodedInputStream;

                const void* data = asio::buffer_cast<const void*>(
                                                        _buffer.data()
                                                                 );
                ArrayInputStream as(data, _len);
                CodedInputStream is(&as);
                is.SetTotalBytesLimit(512 * 1024 * 1024, -1);

                if (!req->ParseFromCodedStream(&is))
                {
                    throw std::runtime_error("ParseFromCodedStream");
                }

                _buffer.consume(_len);

                ProtoBufRpcController *ctrl = new ProtoBufRpcController();
                _service->CallMethod(method, 
                                     ctrl,
                                     req, 
                                     resp, 
                                     NewCallback(
                                             &ProtoBufRpcServiceRequest::run,
                                             new ProtoBufRpcServiceRequest(
                                                           ctrl,
                                                           method,
                                                           req,
                                                           resp,
                                                           shared_from_this())
                                                )
                                     );
                _state = STATE_NONE;
            }
            else
            {
                _state = STATE_WAITING_FOR_DATA;
                start();
            }
        }

    }
    else
    {
        error_code ignored_ec;
        _socket.shutdown(tcp::socket::shutdown_both, ignored_ec);
    }
}

void ProtoBufRpcConnection::handle_write(const error_code& e,
                                         std::size_t bytes_transferred)
{
    if (e)
    {
        error_code ignored_ec;
        _socket.shutdown(tcp::socket::shutdown_both, ignored_ec);
    }
    else
    {
        _buffer.consume(bytes_transferred);

        if (_buffer.size())
        {
            asio::async_write(_socket,
                    _buffer.data(),
                    _strand.wrap(
                        boost::bind(&ProtoBufRpcConnection::handle_write, 
                                    shared_from_this(),
                        asio::placeholders::error,
                        asio::placeholders::bytes_transferred)));
            return;
        }

        _state = STATE_NONE;
        start();
    }
}

ProtoBufRpcServer::ProtoBufRpcServer()
    :_io_service(new asio::io_service())
{
}

bool ProtoBufRpcServer::registerService(uint16_t port,
                                shared_ptr<Service> service)
{
    // This is not thread safe

    // The RegisteredService Constructor fires up the appropriate
    // async accepts for the service
    _services.push_back(shared_ptr<RegisteredService>(
                                        new RegisteredService(
                                                    _io_service,
                                                    port,
                                                    service)));

    return true;
}

void run_wrapper(asio::io_service *io_service)
{
    struct itimerval itimer; 
    setitimer(ITIMER_PROF, &itimer, NULL);

    io_service->run();
}

void ProtoBufRpcServer::run()
{
    try
    {
        if (_services.size() == 0)
        {
            throw std::runtime_error("No services registered for ProtoBufRpcServer");
        }

        size_t nprocs = sysconf(_SC_NPROCESSORS_ONLN);

        vector<shared_ptr<thread> > threads;
        for (size_t i = 0; i < nprocs; ++i)
        {
            shared_ptr<thread> t(new thread(
                                    boost::bind(
                                        //&run_wrapper,
                                        &asio::io_service::run, 
                                        _io_service.get())));
            threads.push_back(t);
        }

        for (size_t i = 0; i < threads.size(); ++i)
        {
            threads[i]->join();
        }
    }
    catch (std::exception &e)
    {
        std::cerr << "ProtoBufRpcService" << e.what() << std::endl;
    }
}

void ProtoBufRpcServer::shutdown()
{
    _io_service->stop();
}

ProtoBufRpcServer::RegisteredService::RegisteredService(
                  shared_ptr<asio::io_service> io_service,
                  uint16_t port,
                  shared_ptr<Service> service
                 )
:_io_service(io_service),
 _port(port),
 _service(service),
 _endpoint(tcp::v4(), _port),
 _acceptor(*_io_service),
 _new_connection(new ProtoBufRpcConnection(*_io_service, _service.get()))
{
    _acceptor.open(_endpoint.protocol());
    _acceptor.set_option(tcp::acceptor::reuse_address(true));
    _acceptor.bind(_endpoint);
    _acceptor.listen();
    _acceptor.async_accept(_new_connection->socket(),
                   boost::bind(&ProtoBufRpcServer::RegisteredService::handle_accept, 
                               this, 
                               asio::placeholders::error));
}

void ProtoBufRpcServer::RegisteredService::handle_accept(const error_code& e)
{
      if (!e)
      {
          _new_connection->start();
          _new_connection.reset(new ProtoBufRpcConnection(*_io_service, _service.get()));
          _acceptor.async_accept(_new_connection->socket(),
                 boost::bind(&ProtoBufRpcServer::RegisteredService::handle_accept,
                             this,
                             asio::placeholders::error));
      }

}

ProtoBufRpcController::ProtoBufRpcController()
{
}

ProtoBufRpcController::~ProtoBufRpcController()
{
}

void ProtoBufRpcController::Reset()
{
}

bool ProtoBufRpcController::Failed() const
{
    return false;
}

string ProtoBufRpcController::ErrorText() const
{
    return "No Error";
}

void ProtoBufRpcController::StartCancel()
{
}

void ProtoBufRpcController::SetFailed(const string &/*reason*/)
{
}

bool ProtoBufRpcController::IsCanceled() const
{
    return false;
}

void ProtoBufRpcController::NotifyOnCancel(Closure * /*callback*/)
{
}

class ProtoBufRpcChannel::MethodHandler 
    : public enable_shared_from_this<MethodHandler>,
      private boost::noncopyable
{
public:
    MethodHandler(auto_ptr<SocketCheckout> socket,
                           const MethodDescriptor * method,
                           RpcController * controller,
                           const Message * request,
                           Message * response,
                           Closure * done
                          )
        :_socket(socket),
        _method(method),
        _controller(controller),
        _request(request),
        _response(response),
        _done(done)
    {
    }

    ~MethodHandler()
    {
        _socket.reset();
        _done->Run();
    }

    static void execute(shared_ptr<MethodHandler> this_ptr)
    {
        int index = htonl(this_ptr->_method->index());
        int rlen = this_ptr->_request->ByteSize();
        int len = htonl(rlen);

        int mlen = sizeof(index) + sizeof(len) + rlen;

        void * data = asio::buffer_cast<void*>(this_ptr->_buffer.prepare(mlen));

        data = void_write(data, index);
        data = void_write(data, len);

        using google::protobuf::io::ArrayOutputStream;

        ArrayOutputStream as(data, rlen);

        this_ptr->_request->SerializeToZeroCopyStream(&as);
        this_ptr->_buffer.commit(mlen);

        (*(this_ptr->_socket))->async_send(this_ptr->_buffer.data(),
                               boost::bind(&ProtoBufRpcChannel::MethodHandler::handle_write,
                                           this_ptr,
                                           asio::placeholders::error,
                                           asio::placeholders::bytes_transferred));
    }

    static void handle_write(shared_ptr<MethodHandler> this_ptr,
                      const error_code& e, 
                      std::size_t bytes_transferred)
    {
        if (!e)
        {
            this_ptr->_buffer.consume(bytes_transferred);

            if (this_ptr->_buffer.size())
            {
                (*(this_ptr->_socket))->async_send(this_ptr->_buffer.data(),
                                       boost::bind(&ProtoBufRpcChannel::MethodHandler::handle_write,
                                                   this_ptr,
                                                   asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred));
                return;
            }

            (*(this_ptr->_socket))->async_receive(
                                      buffer(&this_ptr->_len, sizeof(this_ptr->_len)),
                                      boost::bind(
                                                  &ProtoBufRpcChannel::MethodHandler::handle_read_len,
                                                  this_ptr,
                                                  asio::placeholders::error,
                                                  asio::placeholders::bytes_transferred)
                                     );
        }
        else
        {
            this_ptr->_controller->SetFailed(e.message());
            (*(this_ptr->_socket))->close();
        }
    }

    static void handle_read_len(shared_ptr<MethodHandler> this_ptr,
                                const error_code& e,
                                std::size_t bytes_transferred)
    {
        if (!e && bytes_transferred == sizeof(this_ptr->_len))
        {
            this_ptr->_len = ntohl(this_ptr->_len);
            (*(this_ptr->_socket))->async_receive(
                                      this_ptr->_buffer.prepare(this_ptr->_len),
                                      boost::bind(
                                                  &ProtoBufRpcChannel::MethodHandler::handle_read_response,
                                                  this_ptr,
                                                  asio::placeholders::error,
                                                  asio::placeholders::bytes_transferred
                                                 )
                                     );
        }
        else
        {
            this_ptr->_controller->SetFailed(e.message());
            (*(this_ptr->_socket))->close();
        }
    }

    static void handle_read_response(shared_ptr<MethodHandler> this_ptr,
                              const error_code& e, 
                              std::size_t bytes_transferred)
    {
        if (!e)
        {
            this_ptr->_buffer.commit(bytes_transferred);
            if (this_ptr->_buffer.size() >= this_ptr->_len)
            {
                using google::protobuf::io::ArrayInputStream;
                using google::protobuf::io::CodedInputStream;

                const void* data = asio::buffer_cast<const void*>(
                                                        this_ptr->_buffer.data()
                                                                 );
                ArrayInputStream as(data, this_ptr->_len);
                CodedInputStream is(&as);
                is.SetTotalBytesLimit(512 * 1024 * 1024, -1);

                if (!this_ptr->_response->ParseFromCodedStream(&is))
                {
                    throw std::runtime_error("ParseFromCodedStream");
                }

                this_ptr->_buffer.consume(this_ptr->_len);
            }
            else
            {
                (*(this_ptr->_socket))->async_receive(
                                          this_ptr->_buffer.prepare(this_ptr->_len - this_ptr->_buffer.size()),
                                          boost::bind(
                                                      &ProtoBufRpcChannel::MethodHandler::handle_read_response,
                                                      this_ptr,
                                                      asio::placeholders::error,
                                                      asio::placeholders::bytes_transferred
                                                     )
                                         );
                return;
            }
        }
        else
        {
            this_ptr->_controller->SetFailed(e.message());
            (*(this_ptr->_socket))->close();
        }
    }

private:
    auto_ptr<SocketCheckout> _socket;
    const MethodDescriptor * _method;
    RpcController * _controller;
    const Message * _request;
    Message * _response;
    Closure * _done;
    asio::streambuf _buffer;
    unsigned int _len;
    bool _status;
    unsigned int _sent;
};


ProtoBufRpcChannel::ProtoBufRpcChannel(const string &remotehost, 
                                  const string &port)
    :_remote_host(remotehost), _port(port),
     _resolver(_io_service),
     _acceptor(_io_service),
     _pool(2000, _io_service),
     _lame_socket(_io_service),
     _thread()
//                                &asio::io_service::run, 
//                                &_io_service)))
{

    tcp::resolver::query query(_remote_host, _port);
    tcp::resolver::iterator endpoint_iterator = _resolver.resolve(query);
    tcp::resolver::iterator end;

    error_code error = asio::error::host_not_found;

    if (endpoint_iterator == end) throw syserr::system_error(error);

    _pool.setEndpoint(*endpoint_iterator);

    tcp::endpoint e(tcp::v4(), 0);
    _acceptor.open(e.protocol());
    _acceptor.set_option(tcp::acceptor::reuse_address(true));
    _acceptor.bind(e);
    _acceptor.listen();
    _acceptor.async_accept(_lame_socket,
                       boost::bind(&ProtoBufRpcChannel::lame_handle_accept, this, 
                                   asio::placeholders::error));

    _thread = shared_ptr<thread>(new thread(
                                     boost::bind(
                                             &asio::io_service::run, 
                                             &_io_service)));
}

void ProtoBufRpcChannel::lame_handle_accept(const error_code &err)
{
    if (!err)
    {
        _acceptor.async_accept(_lame_socket,
                           boost::bind(&ProtoBufRpcChannel::lame_handle_accept,
                                       this,
                                       asio::placeholders::error));
    }
}

ProtoBufRpcChannel::~ProtoBufRpcChannel()
{
    _pool.cancelAndClear();

    _io_service.stop();

    _thread->join();
}

void ProtoBufRpcChannel::CallMethod(
        const MethodDescriptor * method,
        RpcController * controller,
        const Message * request,
        Message * response,
        Closure * done)
{
    shared_ptr<MethodHandler> h(
                            new MethodHandler(
                          auto_ptr<SocketCheckout>(new SocketCheckout(&_pool)),
                                              method,
                                              controller,
                                              request,
                                              response,
                                              done
                                             ));

    MethodHandler::execute(h);
}

} // namespace bicker
