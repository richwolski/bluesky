#ifndef _KVSERVICE_H_
#define _KVSERVICE_H_ 1

#include "kvstore.pb.h"
#include "backend.h"

#include <memory>

using std::auto_ptr;

namespace kvstore
{
    class KeyValueRpcService : public ::kvrpc::KeyValueService
    {
    public:
        KeyValueRpcService(Backend *backend);

        virtual ~KeyValueRpcService();

        virtual void PutValue(::google::protobuf::RpcController* controller,
                              const ::kvrpc::Put* request,
                              ::kvrpc::PutReply* response,
                              ::google::protobuf::Closure* done);
        virtual void GetValue(::google::protobuf::RpcController* controller,
                              const ::kvrpc::Get* request,
                              ::kvrpc::GetReply* response,
                              ::google::protobuf::Closure* done);
    private:
        auto_ptr<Backend> _backend;

    };
} // namespace kvstore

#endif
