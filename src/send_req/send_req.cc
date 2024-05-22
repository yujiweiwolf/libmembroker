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
#define NUM_ORDER 1

const char fund_id[] = "S1";
const char mem_dir[] = "../data";
const char mem_req_file[] = "broker_req";
const char mem_rep_file[] = "broker_rep";


void write_order(x::MMapWriter* writer) {
    for (int index = 0; index < NUM_ORDER; index++) {
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
            order->volume = 100 * ( index + 1);
            order->price = 10.01 + 0.01 * i;
            order->price_type = kQOrderTypeLimit;
            sprintf(order->code, "00000%d.SZ", index + 1);
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
    cout << "please input order_no" << endl;
    cin >> msg->order_no;
    LOG_INFO << "send withdraw, fund_id: " << msg->fund_id << ", order_no: " << msg->order_no;
    msg->timestamp = x::RawDateTime();
    writer->CloseFrame(kMemTypeTradeWithdrawReq);
}

void query_asset(x::MMapWriter* writer) {
    string id = x::UUID();
    void* buffer = writer->OpenFrame(sizeof(MemGetTradeAssetMessage));
    MemGetTradeAssetMessage* msg = (MemGetTradeAssetMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id);
    msg->timestamp = x::RawDateTime();
    writer->CloseFrame(kMemTypeQueryTradeAssetReq);
}

void query_position(x::MMapWriter* writer) {
    string id = x::UUID();
    void* buffer = writer->OpenFrame(sizeof(MemGetTradePositionMessage));
    MemGetTradePositionMessage* msg = (MemGetTradePositionMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id);
    msg->timestamp = x::RawDateTime();
    writer->CloseFrame(kMemTypeQueryTradePositionReq);

}

void query_knock(x::MMapWriter* writer) {
    string id = x::UUID();
    void* buffer = writer->OpenFrame(sizeof(MemGetTradeKnockMessage));
    MemGetTradeKnockMessage* msg = (MemGetTradeKnockMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id);
    msg->timestamp = x::RawDateTime();
    writer->CloseFrame(kMemTypeQueryTradeKnockReq);
}

void ReadRep() {
    const void* data = nullptr;
    x::MMapReader common_reader;
    common_reader.Open(mem_dir, mem_rep_file, true);
    while (true) {
        x::Sleep(1000);
        int32_t type = common_reader.Next(&data);
        if (type == kMemTypeTradeOrderRep) {
            MemTradeOrderMessage* rep = (MemTradeOrderMessage*)data;
            LOG_INFO << "收到报单响应, " << ToString(rep);
        } else if (type == kMemTypeTradeWithdrawRep) {
            MemTradeWithdrawMessage* rep = (MemTradeWithdrawMessage*) data;
            LOG_INFO << "收到撤单响应, " << ToString(rep);
        } else if (type == kMemTypeTradeKnock) {
            MemTradeKnock* msg = (MemTradeKnock*) data;
            LOG_INFO << "成交推送, " << ToString(msg);
        } else if (type == kMemTypeQueryTradeAssetRep) {
            MemGetTradeAssetMessage* rep = (MemGetTradeAssetMessage*) data;
            auto item = (MemTradeAsset*)((char*)rep + sizeof(MemGetTradeAssetMessage));
            for (int i = 0; i < rep->items_size; i++) {
                MemTradeAsset *asset = item + i;
                LOG_INFO << "查询资金响应, fund_id: " << fund_id
                         << ", timestamp: " << asset->timestamp
                         << ", balance: " << asset->balance
                         << ", usable: " << asset->usable
                         << ", margin: " << asset->margin
                         << ", equity: " << asset->equity
                         << ", long_margin_usable: " << asset->long_margin_usable
                         << ", short_margin_usable: " << asset->short_margin_usable
                         << ", short_return_usable: " << asset->short_return_usable;
            }
        } else if (type == kMemTypeQueryTradePositionRep) {
            MemGetTradePositionMessage* rep = (MemGetTradePositionMessage*) data;
            auto item = (MemTradePosition*)((char*)rep + sizeof(MemGetTradePositionMessage));
            for (int i = 0; i < rep->items_size; i++) {
                MemTradePosition *position = item + i;
                LOG_INFO << "查询持仓响应, fund_id: " << fund_id
                         << ", timestamp: " << rep->timestamp
                         << ", code: " << position->code
                         << ", long_volume: " << position->long_volume
                         << ", long_market_value: " << position->long_market_value
                         << ", long_can_close: " << position->long_can_close
                         << ", short_volume: " << position->short_volume
                         << ", short_market_value: " << position->short_market_value
                         << ", short_can_open: " << position->short_can_open;
            }
        } else if (type == kMemTypeQueryTradeKnockRep) {
            MemGetTradeKnockMessage* rep = (MemGetTradeKnockMessage*) data;
            auto item = (MemTradeKnock*)((char*)rep + sizeof(MemGetTradeKnockMessage));
            for (int i = 0; i < rep->items_size; i++) {
                MemTradeKnock *knock = item + i;
                LOG_INFO << "查询成交响应, fund_id: " << knock->fund_id
                         << ", code: " << knock->code
                         << ", match_no: " << knock->match_no
                         << ", timestamp: " << knock->timestamp
                         << ", order_no: " << knock->order_no
                         << ", batch_no: " << knock->batch_no
                         << ", bs_flag: " << knock->bs_flag
                         << ", match_type: " << knock->match_type
                         << ", match_volume: " << knock->match_volume
                         << ", match_price: " << knock->match_price
                         << ", match_amount: " << knock->match_amount;
            }
        } else if (type == kMemTypeMonitorRisk) {
            MemMonitorRiskMessage* msg = (MemMonitorRiskMessage*) data;
            LOG_ERROR << "Risk, " << msg->error << ", timestamp: " << msg->timestamp;
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        std::thread t1(ReadRep);
        x::MMapWriter req_writer;
        req_writer.Open(mem_dir, mem_req_file, kReqMemSize << 20, true);

        x::MMapWriter inner_writer;
        inner_writer.Open("../data", kInnerBrokerFile, kInnerBrokerMemSize << 20, true);
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
                query_asset(&inner_writer);
                break;
            }
            case '6':
            {
                query_position(&inner_writer);
                break;
            }
            case '7':
            {
                break;
            }
            case '8':
            {
                query_knock(&inner_writer);
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
