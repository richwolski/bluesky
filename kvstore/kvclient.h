#ifndef _KVCLIENT_H_
#define _KVCLIENT_H_ 1

#include "kvstore.pb.h"
#include "workqueue.h"
#include "protobufrpc.h"

using namespace bicker;

namespace kvstore
{
    class KeyValueClientRouter;

    class KeyValueClient
    {
    public:
        KeyValueClient(const string& host,
                       const string& port);

        KeyValueClient(const list<string> &hosts);

        bool Put(const string& key,
                 const string& value);

        bool Put(const string& key,
                 const string& value,
                 TaskNotification &tn);

        bool Get(const string& key, string* value);

        bool Get(const string& key,
                 string* value,
                 TaskNotification &tn);

    private:
        shared_ptr<KeyValueClientRouter> _router;
    };
} // namespace kvstore

#endif
