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

const char fund_id[] = "S1";

void write_order(x::MMapWriter* writer) {
    int total_order_num = 1;
    string id = x::UUID();
    void* buffer = writer->OpenFrame(sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_order_num);
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id);
    msg->bs_flag = kBsFlagBuy;
    msg->items_size = total_order_num;
    msg->timestamp = x::RawDateTime();
    MemTradeOrder* item = (MemTradeOrder*)((char*)buffer + sizeof(MemTradeOrderMessage));
    for (int i = 0; i < total_order_num; i++) {
        MemTradeOrder* order = item + i;
        order->volume = 100 * ( i + 1);
        order->price = 10 + 0.01 * i;
        order->price_type = kQOrderTypeLimit;
        sprintf(order->code, "00000%d.SZ", i + 1);
    }
    writer->CloseFrame(kMemTypeTradeOrderReq);
}

void write_withdraw(x::MMapWriter* writer) {
    string id = x::UUID();
    void* buffer = writer->OpenFrame(sizeof(MemTradeWithdrawMessage));
    MemTradeWithdrawMessage* msg = (MemTradeWithdrawMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id);
    strcpy(msg->order_no, "2_600000");
    msg->timestamp = x::RawDateTime();
    writer->CloseFrame(kMemTypeTradeWithdrawReq);
}


void query_asset(x::MMapWriter* writer) {

}

void query_position(x::MMapWriter* writer) {

}


void query_knock(x::MMapWriter* writer) {

}

int main(int argc, char* argv[]) {
    try {
//        {
//            BrokerOptionsPtr options = Config::Instance()->options();
//            MemBrokerServer server;
//            shared_ptr<TestBroker> broker = make_shared<TestBroker>();
//            server.Init(options, broker);
//            server.Run();
//        }
//        return 0;
        int64_t now = 20240322023000000;
        int64_t after = 20240322023100000;
        int64_t ms = x::SubRawDateTime(after, now);
        MemBrokerOptionsPtr options = Config::Instance()->options();
        x::MMapWriter req_writer;
        req_writer.Open(options->mem_dir(), options->mem_req_file(), kReqMemSize << 20, true);
        MemBrokerServer server;
        shared_ptr<TestBroker> broker = make_shared<TestBroker>();
        server.Init(options, broker);
        server.Start();
        string usage("\nTYPE  'q' to quit program\n");
        usage += "      '1' to order_sh\n";
        usage += "      '2' to order_sz\n";
        usage += "      '3' to withdraw_sh\n";
        usage += "      '4' to withdraw_sz\n";
        usage += "      '5' to query asset\n";
        usage += "      '6' to query position\n";
        usage += "      '7' to query order\n";
        usage += "      '8' to query knock\n";

        cerr << (usage);

        char c;
        while ((c = getchar()) != 'q') {
            switch (c) {
            case '1':
            {
                write_order(&req_writer);
                break;
            }
            case '2':
            {
                break;
            }
            case '3':
            {
                write_withdraw(&req_writer);
                break;
            }
            case '4':
            {
                break;
            }
            case '5':
            {
                query_asset(&req_writer);
                break;
            }
            case '6':
            {
                query_position(&req_writer);
                break;
            }
            case '7':
            {
                break;
            }
            case '8':
            {
                query_knock(&req_writer);
                break;
            }
            default:
                break;
            }
        }


    } catch (std::exception& e) {
        LOG_FATAL << "server is crashed, " << e.what();
        throw e;
    }
    return 0;
}
