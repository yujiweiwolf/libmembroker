#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <boost/program_options.hpp>
#include "test_broker.h"
#include "config.h"

using namespace std;
using namespace co;
namespace po = boost::program_options;

int main(int argc, char* argv[]) {
    try {
        MemBrokerOptionsPtr options = Config::Instance()->options();
        MemBrokerServer server;
        shared_ptr<TestBroker> broker = make_shared<TestBroker>();
        server.Init(options, broker);
        server.Run();
        while (true) {
            x::Sleep(1000);
        }
    } catch (std::exception& e) {
        LOG_FATAL << "server is crashed, " << e.what();
        throw e;
    }
    return 0;
}
