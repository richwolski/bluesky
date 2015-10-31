#include "backend.h"
#include "util.h"

#include <iostream>
#include <boost/functional/hash.hpp>

/* For bdb */
extern "C" {
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
}


namespace kvstore
{

MemoryBackend::~MemoryBackend()
{
}

bool MemoryBackend::Put(const string &key,
                        const string &value)
{
    _map[key] = value;
    return true;
}

bool MemoryBackend::Get(const string &key,
                        string *value)
{
    map_t::iterator it = _map.find(key);

    if (it == _map.end())
    {
        return false;
    }
    else
    {
        *value = it->second;
        return true;
    }
}

void set_DBT(DBT *thing, const string &str)
{
    bzero(thing, sizeof(DBT));
    thing->data = (void*)str.data();
    thing->size = str.size();
    thing->ulen = str.size();
}

class BDBBackend::BDBDatabase
{
public:
    BDBDatabase(const string &path,
               bool flush,
               bool log_in_memory,
               size_t num_dbs)
        :_path(path)
    {
        int res = db_env_create(&_dbenv, 0);

        if (res != 0)
        {
            cerr << db_strerror(res) << endl;
            throw std::runtime_error("db_env_create fail");
        }

        /* Set Cache Size To Total Memory */
        if (true)
        {
            double use_fraction = 0.1;
            uint64_t pages = sysconf(_SC_PHYS_PAGES);
            uint64_t page_size = sysconf(_SC_PAGE_SIZE);

            uint64_t bytes = pages * page_size * use_fraction / num_dbs;

            uint32_t gbytes = bytes / (1024uLL * 1024uLL * 1024uLL);
            uint32_t nbytes = bytes % (1024uLL * 1024uLL * 1024uLL);
            uint32_t ncache = bytes / (1024uLL * 1024uLL * 1024uLL * 4uLL) + 1;

            res = _dbenv->set_cachesize(_dbenv, gbytes, nbytes, ncache);

            if (res != 0)
            {
                cerr << db_strerror(res) << endl;
                throw std::runtime_error("set_cachesize");
            }
        }

        if (log_in_memory)
        {
            res = _dbenv->set_flags(_dbenv, DB_LOG_IN_MEMORY, 1);

            if (res != 0)
            {
                cerr << db_strerror(res) << endl;
                throw std::runtime_error("BDB ENV DB_LOG_IN_MEMORY");
            }

        }

        res = _dbenv->set_flags(_dbenv, DB_LOG_AUTO_REMOVE, 1);

        if (res != 0)
        {
            cerr << db_strerror(res) << endl;
            throw std::runtime_error("BDB ENV DB_LOG_AUTO_REMOVE");
        }

        res = _dbenv->open(_dbenv,
                           _path.c_str(),
                           DB_INIT_CDB
                           | DB_INIT_MPOOL
                           | DB_CREATE
                           | DB_THREAD,
                           0644);


        if (res != 0)
        {
            cerr << db_strerror(res) << endl;
            throw std::runtime_error("BDB ENV Open Fail");
        }

        string dbfilename = _path + "/test.db";

        /* Flush */
        if (flush)
        {
            res = _dbenv->dbremove(_dbenv, NULL, dbfilename.c_str(), "test", 0);

            if (res != 0 && res != ENOENT)
            {
                cerr << db_strerror(res) << endl;
                throw std::runtime_error("db remove failed");
            }
        }

        res = db_create(&_db, _dbenv, 0);

        if (res != 0)
        {
            cerr << db_strerror(res) << endl;
            throw std::runtime_error("db_create fail");
        }

        uint32_t flags = DB_CREATE | DB_THREAD;

        res = _db->open(_db,
                       NULL, /* TXN */
                       dbfilename.c_str(),
                       "test",
                       DB_BTREE,
                       flags,
                       0644);

        if (res != 0)
        {
            cerr << db_strerror(res) << endl;
            throw std::runtime_error("BDB Open Fail");
        }


    }

    ~BDBDatabase()
    {
        int res;

        if (_db)
        {
            if ((res = _db->close(_db, 0)) < 0)
            {
                cerr << db_strerror(res) << endl;
            }
        }

        if (_dbenv)
        {
            if ((res = _dbenv->close(_dbenv, 0)) < 0)
            {
                cerr << db_strerror(res) << endl;
            }
        }
    }

    bool Put(const string &key,
                     const string &value)
    {
        DBT bdb_key, bdb_value;

        set_DBT(&bdb_key, key);
        set_DBT(&bdb_value, value);

        if (_db->put(_db, NULL, &bdb_key, &bdb_value, 0) != 0)
        {
            perror("bdb put");
            return false;
        }
        else
        {
            return true;
        }
    }

    bool Get(const string &key,
             string *value)
    {
        DBT bdb_key, bdb_value;

        set_DBT(&bdb_key, key);

        bzero(&bdb_value, sizeof(DBT));
        bdb_value.flags = DB_DBT_MALLOC;

        int res = _db->get(_db, NULL, &bdb_key, &bdb_value, 0);

        if (res == 0)
        {
            *value = string((char*)bdb_value.data, bdb_value.size);
            free(bdb_value.data);
            bdb_value.data = NULL;
            return true;
        }
        else if (res == DB_NOTFOUND || res == DB_KEYEMPTY)
        {
            return false;
        }
        else
        {
            /* ERR */
            cerr << db_strerror(res) << endl;
            return false;
        }
    }

private:
    DB *_db;
    DB_ENV *_dbenv;
    const string _path;
};

BDBBackend::BDBBackend(const vector<string> &paths,
                       bool flush,
                       bool log_in_memory)
{
    if (paths.size() < 1)
    {
        cerr << "Insufficient BDB Paths supplied (need at least 1)";
        throw std::runtime_error("not enough paths");
    }


    for (size_t i = 0; i < paths.size(); ++i)
    {
        cerr << "db for " << paths[i] << endl;
        _dbs.push_back(shared_ptr<BDBDatabase>(new BDBDatabase(paths[i],
                                                               flush,
                                                               log_in_memory,
                                                               paths.size())));
    }
}

BDBBackend::~BDBBackend()
{
}


static boost::hash<string> hasher;

inline size_t BDBBackend::LookupKeyDB(const string &key)
{
    uint32_t hash = (uint32_t)hasher(key);
    return hash % _dbs.size();
}

bool BDBBackend::Put(const string &key,
                 const string &value)
{
    size_t i = LookupKeyDB(key);

    return _dbs[i]->Put(key, value);
}

bool BDBBackend::Get(const string &key,
                 string *value)
{
    size_t i = LookupKeyDB(key);
    return _dbs[i]->Get(key, value);
}


} // namespace kvstore
