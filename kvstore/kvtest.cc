#include <boost/shared_ptr.hpp>
#include <gtest/gtest.h>
#include "workqueue.h"
#include "kvservice.h"
#include "kvclient.h"
#include "protobufrpc.h"

using namespace boost;
using namespace kvstore;
using namespace bicker;
using namespace kvrpc;

/* http://code.google.com/p/googletest/wiki/GoogleTestPrimer */

class KVTest : public ::testing::Test
{
public:
    KVTest()
        :_kv_client("127.0.0.1", "9090")
    {
        vector<string> data_dirs;
        data_dirs.push_back("/tmp");

        shared_ptr<Service> service(new KeyValueRpcService(new BDBBackend(data_dirs)));
        _rpc_server.registerService(9090, service);

        _server_thread.reset(
                        new thread(
                                boost::bind(
                                        &ProtoBufRpcServer::run, 
                                        &_rpc_server)));

    }

    virtual ~KVTest()
    {
        _rpc_server.shutdown();
        _server_thread->join();
    }
protected:

    ProtoBufRpcServer _rpc_server;
    KeyValueClient _kv_client;
    shared_ptr<thread> _server_thread;
};


TEST_F(KVTest, PutTest)
{
    ASSERT_TRUE(_kv_client.Put("test", "value"));
    string value;
    ASSERT_TRUE(_kv_client.Get("test", &value));
    ASSERT_EQ(value, "value");
}

TEST_F(KVTest, MedPutTest)
{
    string test_value(1024*10, '6');
    ASSERT_TRUE(_kv_client.Put("test", test_value));
    string value;
    ASSERT_TRUE(_kv_client.Get("test", &value));
    ASSERT_TRUE(value == test_value);
}

TEST_F(KVTest, BigPutTest)
{
    string test_value(1024*1024*10, '6');
    ASSERT_TRUE(_kv_client.Put("test", test_value));
    string value;
    ASSERT_TRUE(_kv_client.Get("test", &value));
    ASSERT_EQ(value.size(), test_value.size());
    ASSERT_TRUE(value == test_value);
}

TEST_F(KVTest, LoopPutTest)
{
    string test_value(1024*1024, '6');

    for (int i = 0; i < 10; ++i)
    {
        ostringstream key;
        key << "test" << i;
        ASSERT_TRUE(_kv_client.Put(key.str(), test_value));
        string value;
        ASSERT_TRUE(_kv_client.Get(key.str(), &value));
        ASSERT_TRUE(value == test_value);
    }
}

TEST_F(KVTest, EmptyTest)
{
    string value;
    ASSERT_FALSE(_kv_client.Get("test", &value));
}

int
main(
     int argc,
     char **argv
    )
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
