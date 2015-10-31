#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <iostream>
#include <string>
#include <ctime>
#include <csignal>
#include "kvservice.h"
#include "protobufrpc.h"

using namespace bicker;
using namespace boost;
using namespace kvstore;
using namespace std;

namespace po = boost::program_options;

int interrupted_count = 0;
ProtoBufRpcServer rpc_server;

void
interrupted(int /*signal*/)
{
    if (interrupted_count < 1)
    {
        rpc_server.shutdown();
    }
    else
    {
        exit(1);
    }
    ++interrupted_count;
}

int
main(
     int argc,
     char **argv
    )
{
    shared_ptr<thread> server_thread;

    /* Initialize Exception Handling */
    struct sigaction action;

    action.sa_handler = &interrupted;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGINT, &action, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    /* Parse Options */
    string opt_bdb_home;
    uint16_t opt_port;
    uint16_t opt_sleep;

    po::options_description options("Options");

    options.add_options()
        ("help,h", "display help message")
        ("bdb-home,d",
         po::value< vector<string> >(),
         "bdb home directories")
        ("port",
         po::value<uint16_t>(&opt_port)->default_value(9090),
         "listen port")
        ("foreground,f", "don't fork to the background")
        ("sleep,s", po::value<uint16_t>(&opt_sleep)->default_value(0), "sleep then exit")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, options), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        cerr << options << endl;
        return 1;
    }


    vector<string> paths;

    if (vm.count("bdb-home") > 0)
    {
        paths = vm["bdb-home"].as< vector<string> >();
    }
    else
    {
        paths.push_back("./");
    }


    shared_ptr<Service> service(new KeyValueRpcService(new BDBBackend(paths)));

    rpc_server.registerService(opt_port, service);

    server_thread.reset(
                         new thread(
                                    boost::bind(
                                                &ProtoBufRpcServer::run, 
                                                &rpc_server)));

    if (!vm.count("foreground"))
    {
        // Daemonize
        if (daemon(0,0) != 0)
        {
            perror("daemonizing");
            exit(1);
        }
    }

    if (opt_sleep == 0)
    {
        server_thread->join();
    }
    else
    {
        time_t t0 = time(NULL);
        time_t t1;

        do
        {
            t1 = time(NULL);
            sleep(opt_sleep - (t1 - t0));
        } while (t1 - t0 < opt_sleep);
    }
    rpc_server.shutdown();

    return 0;
}


