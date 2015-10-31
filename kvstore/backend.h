#ifndef _BACKEND_H_
#define _BACKEND_H_ 1

extern "C"
{
#include <db.h>
}

#include <string>
#include <map>

#include <boost/shared_ptr.hpp>

#include "util.h"

using namespace std;
using boost::shared_ptr;

namespace kvstore
{

class Backend
{
public:
    virtual ~Backend() {};

    virtual bool Put(const string &key,
                     const string &value) = 0;

    virtual bool Get(const string &key,
                     string *value) = 0;

};

class MemoryBackend : public Backend
{
public:
    virtual ~MemoryBackend();

    virtual bool Put(const string &key,
                     const string &value);

    virtual bool Get(const string &key,
                     string *value);

private:
    boost::mutex _lock;
    typedef map<string, string> map_t;
    map_t _map;
};

class BDBBackend : public Backend
{
public:
    BDBBackend(const vector<string> &paths,
               bool flush=true,
               bool log_in_memory = false);

    virtual ~BDBBackend();
    virtual bool Put(const string &key,
                     const string &value);

    virtual bool Get(const string &key,
                     string *value);

private:
    class BDBDatabase;

    inline size_t LookupKeyDB(const string &key);

    vector< shared_ptr<BDBDatabase> > _dbs;
};

} // namespace kvstore

#endif
