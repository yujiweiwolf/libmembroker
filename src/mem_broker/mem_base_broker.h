// Copyright 2025 Fancapital Inc.  All rights reserved.
#pragma once
#include "options.h"
#include "mem_struct.h"
#include "inner_option_master.h"
#include "inner_stock_master.h"
#include "inner_future_master.h"

namespace co {

class MemBrokerServer;

class MemBroker {
 public:
    MemBroker();
    virtual ~MemBroker();

    void Init(const MemBrokerOptions& opt, MemBrokerServer* server);
    void AddT0Code(const string& code);
    void SetAccount(const MemTradeAccount& account);

    // 查询和报撤单请求
    void SendQueryTradeAsset(MemGetTradeAssetMessage* req);
    void SendQueryTradePosition(MemGetTradePositionMessage* req);
    void SendQueryTradeKnock(MemGetTradeKnockMessage* req);
    void SendTradeOrder(MemTradeOrderMessage* req);
    void SendTradeWithdraw(MemTradeWithdrawMessage* req);

    void SendRtnMessage(const std::string& raw, int64_t type);

    // 自动开平， 启始化时查询持仓
    void OnStart();
    void InitPositions(MemGetTradePositionMessage* rep, int64_t type);
    void HandleTradeOrderRep(MemTradeOrderMessage* rep);
    void HandleTradeKnock(MemTradeKnock* knock);

 protected:
    virtual void OnInit();

    virtual void OnQueryTradeAsset(MemGetTradeAssetMessage* req);

    virtual void OnQueryTradePosition(MemGetTradePositionMessage* req);

    virtual void OnQueryTradeKnock(MemGetTradeKnockMessage* req);

    virtual void OnTradeOrder(MemTradeOrderMessage* req);

    virtual void OnTradeWithdraw(MemTradeWithdrawMessage* req);

    int64_t CheckTimeout(int64_t request_time, int64_t ttl_ms);

 private:
    MemBrokerServer* server_ = nullptr;
    MemTradeAccount account_;
    bool enable_stock_short_selling_ = false;  // 是否启用股票自动融券买卖
    int64_t request_timeout_ms_ = 0;
    InnerFutureMaster inner_future_master_;  // 期货内部持仓管理
    InnerOptionMaster inner_option_master_;  // 期权内部持仓管理
    InnerStockMaster inner_stock_master_;    // 信用broker, 股票内部持仓管理
};

typedef std::shared_ptr<MemBroker> MemBrokerPtr;
}  // namespace co

