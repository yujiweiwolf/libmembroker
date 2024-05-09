
#include <regex>
#include <iomanip>
#include <boost/filesystem.hpp>
#include "x/x.h"
#include "coral/coral.h"
#include "mem_processor.h"
#include "mem_server.h"

namespace co {

    MemProcessor::MemProcessor(MemBrokerServer* server): server_(server) {

    }

    MemProcessor::~MemProcessor() {

    }

    void MemProcessor::Init(const MemBrokerOptions& opt) {
        string mem_dir = opt.mem_dir();
        string mem_req_file = opt.mem_req_file();
        string mem_rep_file = opt.mem_rep_file();
        string inner_broker_file = kInnerBrokerFile;
        cpu_affinity_ = opt.cpu_affinity();

        bool exit_flag = false;
        if (boost::filesystem::exists(mem_dir)) {
            boost::filesystem::path p(mem_dir);
            for (auto &file : boost::filesystem::directory_iterator(p)) {
                const string filename = file.path().filename().string();
                if (filename.find(mem_req_file) != filename.npos) {
                    exit_flag = true;
                    break;
                }
            }
        }
        if (!exit_flag) {
            x::MMapWriter req_writer;
            req_writer.Open(mem_dir, mem_req_file, kReqMemSize << 20, true);
        }
        consume_reader_.SetEnableConsume(true);
        consume_reader_.Open(mem_dir, mem_req_file, true);

        common_reader_.Open(mem_dir, mem_rep_file, false);  // 响应结果从头开始读取
        common_reader_.Open("../data", inner_broker_file, true);
    }

    void MemProcessor::Run() {
        if (cpu_affinity_ > 0) {
            x::SetCPUAffinity(cpu_affinity_);
        }
        const void* data = nullptr;
        auto get_req = [&](int32_t type, const void* data)-> bool {
            if (type == kMemTypeTradeOrderReq) {
                MemTradeOrderMessage *msg = (MemTradeOrderMessage *)data;
                if (server_->ExitAccout(msg->fund_id)) {
                    return true;
                } else {
                    return false;
                }
            } else if (type == kMemTypeTradeWithdrawReq) {
                MemTradeWithdrawMessage *msg = (MemTradeWithdrawMessage *)data;
                if (server_->ExitAccout(msg->fund_id)) {
                    return true;
                } else {
                    return false;
                }
            }
            return false;
        };
        while (true) {
            // 抢占式读网关的报撤单数据
            while (true) {
                int32_t type = consume_reader_.ConsumeWhere(&data, get_req, true);
                // int32_t type = consume_reader_.Next(&data);
                if (type == kMemTypeTradeOrderReq) {
                    LOG_INFO << "type: " << type;
                    MemTradeOrderMessage* msg = (MemTradeOrderMessage*) data;
                    server_->SendTradeOrder(msg);
                } else if (type == kMemTypeTradeWithdrawReq) {
                    LOG_INFO << "type: " << type;
                    MemTradeWithdrawMessage* msg = (MemTradeWithdrawMessage*) data;
                    server_->SendTradeWithdraw(msg);
                } else if (type == 0) {
                    break;
                }
            }
            //////
            while (true) {
                int32_t type = common_reader_.Next(&data);
                if (type == kMemTypeTradeOrderRep) {
                    MemTradeOrderMessage* msg = (MemTradeOrderMessage*) data;
                    server_->SendTradeOrderRep(msg);
                } else if (type == kMemTypeTradeWithdrawRep) {
                    MemTradeWithdrawMessage* msg = (MemTradeWithdrawMessage*) data;
                    server_->SendTradeWithdrawRep(msg);
                } else if (type == kMemTypeTradeKnock) {
                    MemTradeKnock* msg = (MemTradeKnock*) data;
                    server_->SendTradeKnock(msg);
                } else if (type == kMemTypeQueryTradeAssetRep) {
                    MemGetTradeAssetMessage* msg = (MemGetTradeAssetMessage*) data;
                    server_->SendQueryTradeAssetRep(msg);
                } else if (type == kMemTypeQueryTradePositionRep) {
                    MemGetTradePositionMessage* msg = (MemGetTradePositionMessage*) data;
                    server_->SendQueryTradePositionRep(msg);
                } else if (type == kMemTypeQueryTradeKnockRep) {
                    MemGetTradeKnockMessage* msg = (MemGetTradeKnockMessage*) data;
                    server_->SendQueryTradeKnockRep(msg);
                } else if (type == kMemTypeInnerCyclicSignal) {
                    server_->HandInnerCyclicSignal();
                } else if (type == kMemTypeQueryTradeAssetReq) {
                    MemGetTradeAssetMessage* msg = (MemGetTradeAssetMessage*) data;
                    server_->SendQueryTradeAsset(msg);
                } else if (type == kMemTypeQueryTradePositionReq) {
                    MemGetTradePositionMessage* msg = (MemGetTradePositionMessage*) data;
                    server_->SendQueryTradePosition(msg);
                } else if (type == kMemTypeQueryTradeKnockReq) {
                    MemGetTradeKnockMessage* msg = (MemGetTradeKnockMessage*) data;
                    server_->SendQueryTradeKnock(msg);
                } else if (type == 0) {
                    break;
                }
            }
        }
    }
}  // namespace co
