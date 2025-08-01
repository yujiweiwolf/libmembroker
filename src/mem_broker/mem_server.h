// Copyright 2025 Fancapital Inc.  All rights reserved.
#pragma once
#include <vector>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <unordered_map>

#include "x/x.h"
#include "coral/coral.h"
#include "options.h"
#include "mem_base_broker.h"
#include "flow_control.h"
#include "../risker/risk_master.h"

namespace co {
class MemBrokerServer {
 public:
    MemBrokerServer();

    ~MemBrokerServer();

    void Init(MemBrokerOptionsPtr option, const std::vector<std::shared_ptr<RiskOptions>>& risk_opts, MemBrokerPtr broker);

    void Start();
    void Join();
    void Run();

    bool JudgeBrokerAccount(const string& fund_id);
    void SetAccount(const MemTradeAccount& account);
    void BeginTask();
    void EndTask();

    void OnStart();
    void SendRtnMessage(const std::string& raw, int64_t type);

 protected:
    void RunQuery();
    void RunWatch();
    void DoWatch();
    void ReadReqMem();
    void HandleQueueMessage();

    void LoadTradingData();
    bool IsNewMemTradeKnock(MemTradeKnock* knock);
    void CreateInnerMatchNo(MemTradeKnock* knock);

    void SendQueryTradeAsset(MemGetTradeAssetMessage* req);
    void SendQueryTradePosition(MemGetTradePositionMessage* req);
    void SendQueryTradeKnock(MemGetTradeKnockMessage* req);
    void SendTradeOrder(MemTradeOrderMessage* req);
    void SendTradeWithdraw(MemTradeWithdrawMessage* req);

    void SendQueryTradeAssetRep(MemGetTradeAssetMessage* rep);
    void SendQueryTradePositionRep(MemGetTradePositionMessage* rep);
    void SendQueryTradeKnockRep(MemGetTradeKnockMessage* rep);
    void SendTradeOrderRep(MemTradeOrderMessage* rep);
    void SendTradeWithdrawRep(MemTradeWithdrawMessage* rep);
    void SendTradeKnock(MemTradeKnock* knock);
    void SendMonitorRiskMessage(MemMonitorRiskMessage* msg);

 private:
    MemBrokerOptionsPtr opt_;
    MemBrokerPtr broker_;
    RiskMasterPtr risk_;
    std::string node_name_;
    MemTradeAccount account_;
    std::vector<std::shared_ptr<std::thread>> threads_;

    QueryContext asset_context_;
    QueryContext position_context_;
    QueryContext knock_context_;
    int sh_th_tps_limit_ = 1;
    int sz_th_tps_limit_ = 1;

    MemTradeAsset asset_;
    std::unordered_map<std::string, MemTradePosition> positions_;
    std::set<std::string> knocks_;

    int64_t active_task_timestamp_ = 0;
    x::MMapWriter rep_writer_;
    int64_t start_time_ = 0;
    int64_t nature_day_ = 0;
    int64_t wait_size_ = 0;
    int64_t last_heart_beat_ = 0;

    std::unordered_map<std::string, int64_t> pending_orders_;
    std::unordered_map<std::string, int64_t> pending_withdraws_;
    shared_ptr<BrokerQueue> queue_;
    bool enable_flow_control_ = false;
    shared_ptr<FlowControlQueue> flow_control_queue_;
};
}  // namespace co

