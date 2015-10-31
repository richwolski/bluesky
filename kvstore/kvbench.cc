#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <iostream>
#include <string>
#include "kvservice.h"
#include "kvclient.h"
#include "protobufrpc.h"

using namespace bicker;
using namespace boost;
using namespace kvstore;
using namespace std;

namespace po = boost::program_options;

class KVBench
{
public:
    KVBench(const vector<string> &hosts)
        :_kv_client(list<string>(hosts.begin(), hosts.end()))
    {
    }

    virtual ~KVBench()
    {
    }


    void Bench(const size_t size,
               const size_t count)
    {
        string data(size, 'A');

        for (size_t i = 0; i < count; ++i)
        {
            ostringstream key;
            key << "key_" << size << "_" << i << endl;

            _kv_client.Put(key.str(), data);
        }

        for (size_t i = 0; i < count; ++i)
        {
            string value;
            ostringstream key;
            key << "key_" << size << "_" << i << endl;

            _kv_client.Get(key.str(), &value);
        }
    }

protected:
    KeyValueClient _kv_client;
};


int
main(
     int argc,
     char **argv
    )
{
    size_t opt_low;
    size_t opt_high;
    size_t opt_count;

    po::options_description options("Options");

    options.add_options()
        ("help,h", "display help message")
        ("servers",
         po::value< vector<string> >()->multitoken(),
         "server:port ... server:port")
        ("low,l",
         po::value<size_t>(&opt_low)->default_value(1),
         "low 2^i")
        ("high,H",
         po::value<size_t>(&opt_high)->default_value(16),
         "high 2^i")
        ("count,c",
         po::value<size_t>(&opt_count)->default_value(100),
         "count of each size")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, options), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        cerr << options << endl;
        return 1;
    }

    if (!vm.count("servers"))
    {
        cerr << "No Servers Specified" << endl;
        return 1;
    }


    KVBench bench(vm["servers"].as< vector<string> >());

    for (size_t i = opt_low; i <= opt_high; ++i)
    {
        cout << i << ": " << (1<<i) << endl;
        bench.Bench(1<<i, opt_count);
    }

    return 0;
}
