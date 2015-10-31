#include "kvclient.h"
#include <boost/tuple/tuple.hpp>
#include <boost/functional/hash.hpp>

using namespace boost;
using namespace kvrpc;

namespace kvstore
{

class KeyValueClientRouter
{
public:
    KeyValueClientRouter()
    {
    }

    void AddHost(const string &host, const string &port)
    {
        mutex::scoped_lock lock(_lock);

        _channels.push_back(shared_ptr<ProtoBufRpcChannel>(new ProtoBufRpcChannel(host, port)));
        _clients.push_back(shared_ptr<kvrpc::KeyValueService_Stub>(new KeyValueService_Stub(static_cast<RpcChannel*>(_channels.back().get()))));
    }

    kvrpc::KeyValueService_Stub* Route(const string &key)
    {
        static hash<string> hasher;

        uint32_t hash = (uint32_t)hasher(key);
        int id = hash % _clients.size();

        return _clients[id].get();
    }

private:
    mutex _lock;
    vector< shared_ptr<ProtoBufRpcChannel> > _channels;
    vector< shared_ptr<kvrpc::KeyValueService_Stub> > _clients;
};

KeyValueClient::KeyValueClient(const string& host,
                               const string& port)
:_router(new KeyValueClientRouter())
{
    _router->AddHost(host, port);
}

KeyValueClient::KeyValueClient(const list<string> &hosts)
:_router(new KeyValueClientRouter())
{
    for (list<string>::const_iterator i = hosts.begin();
         i != hosts.end();
         ++i)
    {
        size_t pos = i->find(':');
        if (pos == string::npos)
            throw runtime_error("couldn't parse host");

        string host = i->substr(0, pos);
        string port = i->substr(pos+1);

        _router->AddHost(host, port);
    }
}

typedef tuple<shared_ptr<RpcController>,
              shared_ptr<Put>,
              shared_ptr<PutReply> > rpc_put_state_tuple_t;

void cleanupPut(rpc_put_state_tuple_t t, TaskNotification *tn)
{
    tn->completeTask(t.get<2>()->result() == kvrpc::SUCCESS);
}

bool
KeyValueClient::Put(const string& key,
         const string& value)
{
    TaskNotification tn;

    this->Put(key, value, tn);

    tn.waitForComplete();

    return tn.failCount() == 0;
}

bool
KeyValueClient::Put(const string& key,
                    const string& value,
                    TaskNotification &tn)
{
    tn.registerTask();

    shared_ptr<RpcController> ctrl(new ProtoBufRpcController());
    shared_ptr< ::kvrpc::Put> put(new ::kvrpc::Put());

    put->set_key(key);
    put->set_value(value);

    shared_ptr<PutReply> reply(new PutReply());


    _router->Route(key)->PutValue(ctrl.get(), put.get(), reply.get(),
                             NewCallback(&cleanupPut,
                                         rpc_put_state_tuple_t(ctrl,put,reply),
                                         &tn));

    return true;
}

typedef tuple<shared_ptr<RpcController>,
              shared_ptr<Get>,
              shared_ptr<GetReply>,
              string*> rpc_get_state_tuple_t;

void cleanupGet(rpc_get_state_tuple_t t, TaskNotification *tn)
{
    bool result = t.get<2>()->result() == kvrpc::SUCCESS;

    if (result)
    {
        string* result_ptr = t.get<3>();
        *result_ptr = t.get<2>()->value();
    }

    tn->completeTask(result);
}

bool
KeyValueClient::Get(const string& key, string* value)
{
    TaskNotification tn;

    this->Get(key, value, tn);

    tn.waitForComplete();

    return tn.failCount() == 0;
}

bool
KeyValueClient::Get(const string& key, string* value, TaskNotification &tn)
{
    tn.registerTask();

    shared_ptr<RpcController> ctrl(new ProtoBufRpcController());
    shared_ptr< ::kvrpc::Get> get(new ::kvrpc::Get());

    get->set_key(key);

    shared_ptr<GetReply> reply(new GetReply());

    _router->Route(key)->GetValue(ctrl.get(), get.get(), reply.get(),
                             NewCallback(&cleanupGet,
                                         rpc_get_state_tuple_t(ctrl,
                                                               get,
                                                               reply,
                                                               value),
                                         &tn));

    return true;
}

}
