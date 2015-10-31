#include "kvservice.h"
#include <iostream>

extern "C" {
#include <time.h>
#include <sys/time.h>
}

using namespace std;

/* Timing and delay functionality.  We allow the get and put operations to have
 * a specified minimum latency, and will sleep if the operation would have
 * completed before that time.  This can be used in benchmarking to see the
 * effect of increasing latency on an application.
 *
 * Call gettimeofday at the start of the operation to get the starting time,
 * and then use minimum_delay to wait until at least the specified number of
 * microseconds have elapsed. */
static void minimum_delay(const struct timeval *tv, unsigned int min_usec)
{
    struct timeval now;
    if (gettimeofday(&now, NULL) < 0)
        return;

    int64_t t1, t2;             /* Times converted to straight microseconds */
    t1 = (int64_t)tv->tv_sec * 1000000 + tv->tv_usec;
    t2 = (int64_t)now.tv_sec * 1000000 + now.tv_usec;

    unsigned int elapsed = t2 - t1;
    if (elapsed >= min_usec)
        return;

    struct timespec delay;
    delay.tv_sec = (min_usec - elapsed) / 1000000;
    delay.tv_nsec = ((min_usec - elapsed) % 1000000) * 1000;

    while (nanosleep(&delay, &delay) != 0 && errno == EINTR)
        ;
}

namespace kvstore
{

KeyValueRpcService::KeyValueRpcService(Backend *backend)
    :_backend(backend)
{
}

KeyValueRpcService::~KeyValueRpcService()
{
}

void KeyValueRpcService::PutValue(
                      ::google::protobuf::RpcController* /*controller*/,
                      const ::kvrpc::Put* request,
                      ::kvrpc::PutReply* response,
                      ::google::protobuf::Closure* done)
{
    struct timeval start;
    gettimeofday(&start, NULL);

    if (_backend->Put(request->key(), request->value()))
    {
        response->set_result(kvrpc::SUCCESS);
    }
    else
    {
        response->set_result(kvrpc::FAILURE);
    }

    //minimum_delay(&start, 1000000);
    done->Run();
}

void KeyValueRpcService::GetValue(
                      ::google::protobuf::RpcController* /*controller*/,
                      const ::kvrpc::Get* request,
                      ::kvrpc::GetReply* response,
                      ::google::protobuf::Closure* done)
{
    struct timeval start;
    gettimeofday(&start, NULL);

    string value;
    if (_backend->Get(request->key(), &value))
    {
        response->set_result(kvrpc::SUCCESS);
        response->set_value(value);
    }
    else
    {
        response->set_result(kvrpc::FAILURE);
    }

    //minimum_delay(&start, 1000000);
    done->Run();
}

}; // namespace kvstore
