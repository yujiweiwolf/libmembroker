#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <boost/program_options.hpp>
#include "x/x.h"
#include "coral/coral.h"
#include "../mem_broker/mem_base_broker.h"
#include "../mem_broker/mem_server.h"

using namespace std;
using namespace co;
namespace po = boost::program_options;

const char fund_id[] = "S1";
const char mem_dir[] = "../data";
const char mem_req_file[] = "broker_req";
const char mem_rep_file[] = "broker_rep";

void write_order(x::MMapWriter* writer) {
    for (int index = 0; index < 10; index++) {
        int total_order_num = 1;
        string id = x::UUID();
        void* buffer = writer->OpenFrame(sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_order_num);
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*) buffer;
        strncpy(msg->id, id.c_str(), id.length());
        strcpy(msg->fund_id, fund_id);
        msg->bs_flag = kBsFlagBuy;
        msg->items_size = total_order_num;
        MemTradeOrder* item = (MemTradeOrder*)((char*)buffer + sizeof(MemTradeOrderMessage));
        for (int i = 0; i < total_order_num; i++) {
            MemTradeOrder* order = item + i;
            order->volume = 100 * ( i + 1);
            order->price = 10.01 + 0.01 * i;
            order->price_type = kQOrderTypeLimit;
            sprintf(order->code, "00000%d.SZ", i + 1);
            LOG_INFO << "send order, code: " << order->code << ", volume: " << order->volume << ", price: " << order->price;
        }
        msg->timestamp = x::RawDateTime();
        writer->CloseFrame(kMemTypeTradeOrderReq);
    }
}

void write_withdraw(x::MMapWriter* writer) {
    string id = x::UUID();
    void* buffer = writer->OpenFrame(sizeof(MemTradeWithdrawMessage));
    MemTradeWithdrawMessage* msg = (MemTradeWithdrawMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id);
    strcpy(msg->order_no, "2_600000");
    LOG_INFO << "send withdraw, fund_id: " << msg->fund_id << ", order_no: " << msg->order_no;
    msg->timestamp = x::RawDateTime();
    writer->CloseFrame(kMemTypeTradeWithdrawReq);
}

void query_asset(x::MMapWriter* writer) {
    string id = x::UUID();
    MemGetTradeAssetMessage msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.id, id.c_str(), id.length());
    strcpy(msg.fund_id, fund_id);
    msg.timestamp = x::RawDateTime();

}

void query_position(x::MMapWriter* writer) {
    string id = x::UUID();
    MemGetTradePositionMessage msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.id, id.c_str(), id.length());
    strcpy(msg.fund_id, fund_id);
    msg.timestamp = x::RawDateTime();

}

void query_knock(x::MMapWriter* writer) {
    string id = x::UUID();
    MemGetTradeKnockMessage msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.id, id.c_str(), id.length());
    strcpy(msg.fund_id, fund_id);
    msg.timestamp = x::RawDateTime();
}

void Run() {
//    if (cpu_affinity_ > 0) {
//        x::SetCPUAffinity(cpu_affinity_);
//    }
    const void* data = nullptr;
    x::MMapReader common_reader_;
    common_reader_.Open(mem_dir, mem_rep_file, true);
    while (true) {
        int32_t type = common_reader_.Next(&data);
        if (type == kMemTypeTradeOrderRep) {
            MemTradeOrderMessage* rep = (MemTradeOrderMessage*)data;
            {
                int64_t now_time = x::RawDateTime();
                int64_t rep_diff = rep->rep_time - rep->timestamp;
                int64_t read_rep = now_time - rep->rep_time;
                int64_t cross_diff = now_time - rep->timestamp;
                LOG_INFO << "rep_diff: " << rep_diff << ", read_rep: " << read_rep << ", cross_diff: " << cross_diff;
                auto items = (MemTradeOrder*)((char*)rep + sizeof(MemTradeOrderMessage));
                for (int i = 0; i < rep->items_size; i++) {
                    MemTradeOrder* order = items + i;
                    LOG_INFO << "收到报单成交回报, code: " << order->code
                             << ", fund_id: " << rep->fund_id
                             << ", timestamp: " << rep->timestamp
                             << ", rep_time: " << rep->rep_time
                             << ", id: " << rep->id
                             << ", volume: " << order->volume
                             << ", bs_flag: " << rep->bs_flag
                             << ", price: " << order->price
                             << ", order_no: " << order->order_no
                             ;
                }
            }

        } else if (type == kMemTypeTradeWithdrawRep) {
            MemTradeWithdrawMessage* msg = (MemTradeWithdrawMessage*) data;

        } else if (type == kMemTypeTradeKnock) {
            MemTradeKnock* msg = (MemTradeKnock*) data;

        } else if (type == kMemTypeQueryTradeAssetRep) {
            MemGetTradeAssetMessage* msg = (MemGetTradeAssetMessage*) data;

        } else if (type == kMemTypeQueryTradePositionRep) {
            MemGetTradePositionMessage* msg = (MemGetTradePositionMessage*) data;

        } else if (type == kMemTypeQueryTradeKnockRep) {
            MemGetTradeKnockMessage* msg = (MemGetTradeKnockMessage*) data;

        } else if (type == 0) {
            // break;
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        std::thread t1(Run);
        x::MMapWriter req_writer;
        req_writer.Open(mem_dir, mem_req_file, kReqMemSize << 20, false);
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
        t1.join();
    } catch (std::exception& e) {
        LOG_FATAL << "server is crashed, " << e.what();
        throw e;
    }
    return 0;
}
