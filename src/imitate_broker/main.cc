#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <regex>
#include "imitate_broker.h"
#include "config.h"

using namespace std;
using namespace co;
namespace po = boost::program_options;
#define NUM_ORDER 1

const char fund_id[] = "S1";
string mem_dir;
string mem_req_file;
string mem_rep_file;

// BUY 018020.SH 30000 101.9
// BUY 600000.SH 100 9.9
// BUY 159920.SZ 1000 1.119
// SELL 159920.SZ 500 1.100
// BUY 510900.SH 1000 0.800
// SELL 510900.SH 100 0.767

void order(shared_ptr<MemBroker> broker) {
    int total_order_num = 1;
    int64_t bs_flag = 0;
    string id = x::UUID();
    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_order_num;
    char buffer[length] = {};
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
        order->price_type = kQOrderTypeLimit;
        LOG_INFO << "send order, code: " << order->code << ", volume: " << order->volume << ", price: " << order->price;
    }
    msg->bs_flag = bs_flag;
    msg->timestamp = x::RawDateTime();
    broker->SendTradeOrder(msg);
}

void withdraw(shared_ptr<MemBroker> broker) {
    string id = x::UUID();
    MemTradeWithdrawMessage msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.id, id.c_str(), id.length());
    strcpy(msg.fund_id, fund_id);
    cout << "please input order_no" << endl;
    cin >> msg.order_no;
    LOG_INFO << "send withdraw, fund_id: " << msg.fund_id << ", order_no: " << msg.order_no;
    msg.timestamp = x::RawDateTime();
    broker->SendTradeWithdraw(&msg);
}


void query_asset(shared_ptr<MemBroker> broker) {
    string id = x::UUID();
    MemGetTradeAssetMessage msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.id, id.c_str(), id.length());
    strcpy(msg.fund_id, fund_id);
    msg.timestamp = x::RawDateTime();
    broker->SendQueryTradeAsset(&msg);
}

void query_position(shared_ptr<MemBroker> broker) {
    string id = x::UUID();
    MemGetTradePositionMessage msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.id, id.c_str(), id.length());
    strcpy(msg.fund_id, fund_id);
    msg.timestamp = x::RawDateTime();
    broker->SendQueryTradePosition(&msg);
}

void query_knock(shared_ptr<MemBroker> broker) {
    string id = x::UUID();
    MemGetTradeKnockMessage msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.id, id.c_str(), id.length());
    strcpy(msg.fund_id, fund_id);
    msg.timestamp = x::RawDateTime();
    broker->SendQueryTradeKnock(&msg);
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
            req_writer.Open(mem_dir, mem_rep_file, kReqMemSize << 20, true);
        }
    }
    const void* data = nullptr;
    x::MMapReader reader;
    reader.Open(mem_dir, mem_rep_file, true);
    while (true) {
        x::Sleep(1000);
        int32_t type = reader.Next(&data);
        if (type == kMemTypeTradeOrderRep) {
            MemTradeOrderMessage* rep = (MemTradeOrderMessage*)data;
            LOG_INFO << "收到报单响应, " << ToString(rep);
        } else if (type == kMemTypeTradeWithdrawRep) {
            MemTradeWithdrawMessage* rep = (MemTradeWithdrawMessage*) data;
            LOG_INFO << "收到撤单响应, " << ToString(rep);
        } else if (type == kMemTypeTradeKnock) {
            MemTradeKnock* msg = (MemTradeKnock*) data;
            LOG_INFO << "收到成交推送, " << ToString(msg);
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
        MemBrokerOptionsPtr options = Config::Instance()->options();
        const std::vector<std::shared_ptr<RiskOptions>>& risk_opts = Config::Instance()->risk_opt();
        MemBrokerServer server;
        shared_ptr<TestBroker> broker = make_shared<TestBroker>();
        server.Init(options, risk_opts, broker);
        server.Start();

        mem_dir = options->mem_dir();
        mem_req_file = options->mem_req_file();
        mem_rep_file = options->mem_rep_file();
        std::thread t1(ReadRep);
        string usage("\nTYPE  'q' to quit program\n");
        usage += "      '1' to order_sh\n";
        usage += "      '2' to order_sz\n";
        usage += "      '3' to withdraw\n";
        usage += "      '4' to order\n";
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
                    order(broker);
                    break;
                }
                case '2':
                {
                    break;
                }
                case '3':
                {
                    withdraw(broker);
                    break;
                }
                case '4':
                {
                    order(broker);
                    break;
                }
                case '5':
                {
                    query_asset(broker);
                    break;
                }
                case '6':
                {
                    query_position(broker);
                    break;
                }
                case '7':
                {
                    break;
                }
                case '8':
                {
                    query_knock(broker);
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
// namespace co
