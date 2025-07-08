#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <regex>
#include "x/x.h"
#include "coral/coral.h"
#include "../mem_broker/mem_base_broker.h"
#include "../mem_broker/mem_server.h"

using namespace std;
using namespace co;
namespace po = boost::program_options;
#define NUM_ORDER 10

const char fund_id[] = "S1";
const char mem_dir[] = "../data";
const char mem_req_file[] = "broker_req";
const char mem_rep_file[] = "broker_rep";

void order_sh(x::MMapWriter* writer) {
    int total_order_num = 1;
    string id = x::UUID();
    void* buffer = writer->OpenFrame(sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_order_num);
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id);
    msg->items_size = total_order_num;
    msg->bs_flag = co::kBsFlagBuy;
    MemTradeOrder* item = (MemTradeOrder*)((char*)buffer + sizeof(MemTradeOrderMessage));
    for (int i = 0; i < total_order_num; i++) {
        MemTradeOrder* order = item + i;
        strcpy(order->code, "600000.SH");
        order->volume = 100;
        order->price = 9.99;
        string volume;
        cout << "please input volume" << endl;
        cin >> volume;
        order->volume = atoll(volume.c_str());
        LOG_INFO << "send order, code: " << order->code << ", volume: " << order->volume << ", price: " << order->price;
    }
    msg->timestamp = x::RawDateTime();
    writer->CloseFrame(kMemTypeTradeOrderReq);
}

void withdraw(x::MMapWriter* writer) {
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

void order(x::MMapWriter* writer) {
    int total_order_num = 1;
    int64_t bs_flag = 0;
    string id = x::UUID();
    void* buffer = writer->OpenFrame(sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_order_num);
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id);
    msg->items_size = total_order_num;
    MemTradeOrder* item = (MemTradeOrder*)((char*)buffer + sizeof(MemTradeOrderMessage));
    for (int i = 0; i < total_order_num; i++) {
        MemTradeOrder* order = item + i;
        getchar();
        cout << "please input : BUY 600000.SH 100 9.9\n";
        std::string input;
        getline(std::cin, input);
        std::cout << "your input is: # " << input << " #" << std::endl;
        std::smatch result;
        if (regex_match(input, result, std::regex("^(BUY|SELL|CREATE|REDEEM) ([0-9]{1,10})\.(SH|SZ) ([0-9]{1,10}) ([.0-9]{1,10})$")))
        {
            string command = result[1].str();
            string instrument = result[2].str();
            string market = result[3].str();
            string volume = result[4].str();
            string price = result[5].str();
            if (command == "BUY") {
                bs_flag = kBsFlagBuy;
            } else if (command == "SELL") {
                bs_flag = kBsFlagSell;
            }

            string code;
            if (market == "SH") {
                order->market = kMarketSH;
                code = instrument + ".SH";
            } else if (market == "SZ") {
                order->market = kMarketSZ;
                code = instrument + ".SZ";
            }
            strcpy(order->code, code.c_str());
            order->volume = atoll(volume.c_str());
            order->price = atof(price.c_str());
        }
        LOG_INFO << "send order, code: " << order->code << ", volume: " << order->volume << ", price: " << order->price;
    }
    msg->bs_flag = bs_flag;
    msg->timestamp = x::RawDateTime();
    writer->CloseFrame(kMemTypeTradeOrderReq);
}

void ReadRep() {
    {
        bool exit_flag = false;
        if (boost::filesystem::exists(mem_dir)) {
            boost::filesystem::path p(mem_dir);
            for (auto &file : boost::filesystem::directory_iterator(p)) {
                const string filename = file.path().filename().string();
                if (filename.find(mem_rep_file) != filename.npos) {
                    exit_flag = true;
                    break;
                }
            }
        }
        if (!exit_flag) {
            x::MMapWriter req_writer;
            req_writer.Open(mem_dir, mem_rep_file, kReqMemSize << 20, 0, true);
        }
    }
    const void* data = nullptr;
    x::MMapReader reader;
    reader.Open(mem_dir, mem_rep_file, true);
    while (true) {
        x::Sleep(100);
        int32_t type = reader.Next(&data);
        if (type == kMemTypeTradeOrderRep) {
            MemTradeOrderMessage* rep = (MemTradeOrderMessage*)data;
            LOG_INFO << "收到报单响应, " << ToString(rep);
        } else if (type == kMemTypeTradeWithdrawRep) {
            MemTradeWithdrawMessage* rep = (MemTradeWithdrawMessage*) data;
            LOG_INFO << "收到撤单响应, " << ToString(rep);
        } else if (type == kMemTypeTradeKnock) {
            MemTradeKnock* msg = (MemTradeKnock*) data;
            LOG_INFO << "成交, " << ToString(msg);
        } else if (type == kMemTypeTradeAsset) {
            MemTradeAsset *asset = (MemTradeAsset*) data;
            LOG_INFO << "资金变更, fund_id: " << asset->fund_id
                     << ", timestamp: " << asset->timestamp
                     << ", balance: " << asset->balance
                     << ", usable: " << asset->usable
                     << ", margin: " << asset->margin
                     << ", equity: " << asset->equity
                     << ", long_margin_usable: " << asset->long_margin_usable
                     << ", short_margin_usable: " << asset->short_margin_usable
                     << ", short_return_usable: " << asset->short_return_usable;

        } else if (type == kMemTypeTradePosition) {
            MemTradePosition* position = (MemTradePosition*) data;
            LOG_INFO << "持仓变更, code: " << position->code
                     << ", fund_id: " << position->fund_id
                     << ", timestamp: " << position->timestamp
                     << ", long_volume: " << position->long_volume
                     << ", long_market_value: " << position->long_market_value
                     << ", long_can_close: " << position->long_can_close
                     << ", short_volume: " << position->short_volume
                     << ", short_market_value: " << position->short_market_value
                     << ", short_can_open: " << position->short_can_open;
        } else if (type == kMemTypeMonitorRisk) {
            MemMonitorRiskMessage* msg = (MemMonitorRiskMessage*) data;
            LOG_ERROR << "Risk, " << msg->error << ", timestamp: " << msg->timestamp;
        } else if (type == kMemTypeHeartBeat) {
            HeartBeatMessage* msg = (HeartBeatMessage*) data;
            LOG_ERROR << "心跳, " << msg->fund_id << ", timestamp: " << msg->timestamp;
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        std::thread t1(ReadRep);
        x::MMapWriter req_writer;
        req_writer.Open(mem_dir, mem_req_file, kReqMemSize << 20, 0, true);

        string usage("\nTYPE  'q' to quit program\n");
        usage += "      '1' to order_sh\n";
        usage += "      '2' to order_sz\n";
        usage += "      '3' to withdraw\n";
        usage += "      '4' to order\n";
        cerr << (usage);

        char c;
        while ((c = getchar()) != 'q') {
            switch (c) {
            case '1':
            {
                order_sh(&req_writer);
                break;
            }
            case '3':
            {
                withdraw(&req_writer);
                break;
            }
            case '4':
            {
                order(&req_writer);
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
