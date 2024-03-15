#pragma once

#include <string>
#include <queue>
#include <vector>
#include <map>
#include <list>
#include <deque>
#include <memory>
#include <tuple>

#include <boost/asio.hpp>
#include "coral/coral.h"
#include "options.h"
#include "mem_struct.h"

namespace co {

    class MemBrokerServer;

    class MemProcessor {
    public:
        MemProcessor(MemBrokerServer* server);
        ~MemProcessor();

        void Init(const MemBrokerOptions& opt);
        void Run();

        bool alive() const;
        std::string node_id() const;
        std::string node_name() const;

    protected:
        void QueryTradeAsset(MemGetTradeAssetMessage* req);
        void QueryTradePosition(MemGetTradePositionMessage* req);
        void QueryTradeKnock(MemGetTradeKnockMessage* req);
        void SendTradeOrder(MemTradeOrderMessage* req);
        void SendTradeWithdraw(MemTradeWithdrawMessage* req);

        void SendQueryTradeAssetRep(MemTradeAsset* rep);
        void SendQueryTradePositionRep(MemTradePosition* rep);
        // void SendQueryTradeKnockRep(MemTradeKnock* rep);
        void SendTradeOrderRep(MemTradeOrderMessage* rep);
        void SendTradeWithdrawRep(MemTradeWithdrawMessage* rep);
        void SendTradeKnock(MemTradeKnock* knock);

    private:
        std::string fund_id_;
        std::string tag_;
        int64_t node_type_ = 0;
        std::string node_id_; // 节点编号
        std::string node_name_; // 节点名称
        bool quit_ = false;
        bool alive_ = false;

        int cpu_affinity_ = 0;
        MemBrokerServer* server_ = nullptr;
        x::MMapReader consume_reader_;  // 抢占式读网关的报撤单数据
        x::MMapReader common_reader_;  // 查询请求, api回写的交易数据, 有数据就读取
    };
    typedef std::shared_ptr<MemProcessor> MemProcessorPtr;
}