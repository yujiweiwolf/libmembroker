
#include <regex>
#include <iomanip>
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
        string req_mem_file = opt.mem_req_file();
        string rep_mem_file = opt.mem_rep_file();
        string inner_broker_file = kInnerBrokerFile;
        cpu_affinity_ = opt.cpu_affinity();

        consume_reader_.Open(mem_dir, req_mem_file, true);
        common_reader_.Open(mem_dir, rep_mem_file, false);  // 响应结果从头开始读取
        common_reader_.Open("../data", inner_broker_file, true);
    }

    void MemProcessor::Run() {
        if (cpu_affinity_ > 0) {
            x::SetCPUAffinity(cpu_affinity_);
        }
        void* data = nullptr;
        auto get_req = [&](int32_t type, void* data)-> bool {
            if (type == kMemTypeTradeOrderReq) {
                MemTradeOrderMessage *msg = (MemTradeOrderMessage *) data;
                if (server_->ExitAccout(msg->fund_id)) {
                    return true;
                } else {
                    return false;
                }
            } else if (type == kMemTypeTradeWithdrawReq) {
                MemTradeWithdrawMessage *msg = (MemTradeWithdrawMessage *) data;
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
                int32_t type = common_reader_.Read(&data);

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
                } else if (type == kMemTypeInnerHeartBeat) {
                    server_->SendHeartBeat();
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
