// Copyright 2021 Fancapital Inc.  All rights reserved.
#pragma once
#include <map>
#include <string>
#include <memory>
#include "options.h"
#include "mem_struct.h"
#include "inner_option_master.h"
#include "inner_stock_master.h"

namespace co {

class MemBrokerServer;

class MemBroker {
 public:
    MemBroker();
    virtual ~MemBroker();

    void Init(const MemBrokerOptions& opt, MemBrokerServer* server);

    const std::map<string, MemTradeAccount>& GetAccounts() const;
    co::MemTradeAccount* GetAccount(const string& fund_id);
    bool ExitAccount(const string& fund_id);
    void AddAccount(const co::MemTradeAccount& account);

    void AddT0Code(const string& code);

    void SendQueryTradeAsset(MemGetTradeAssetMessage* req);
    void SendQueryTradePosition(MemGetTradePositionMessage* req);
    void SendQueryTradeKnock(MemGetTradeKnockMessage* req);
    void SendTradeOrder(MemTradeOrderMessage* req);
    void SendTradeWithdraw(MemTradeWithdrawMessage* req);

    void SendRtnMessage(const std::string& raw, int64_t type);

    void SetInitPositions(MemGetTradePositionMessage* rep, int64_t type);

    void SendTradeOrderRep(MemTradeOrderMessage* rep);

    // 自动开平，计算持仓
    void SendTradeKnock(MemTradeKnock* knock);

    // 自动开平， 启始化时查询持仓
    void OnStart();

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
    std::map<string, MemTradeAccount> accounts_;  // 支持的所有资金账号，由底层在on_init()初始化函数中填写， fund_id -> trade_type
    bool enable_stock_short_selling_ = false;  // 是否启用股票自动融券买卖
    int64_t request_timeout_ms_ = 0;
    InnerOptionMaster inner_option_master_;  // 期权内部持仓管理
    InnerStockMaster inner_stock_master_;  // 信用broker, 股票内部持仓管理
};

typedef std::shared_ptr<MemBroker> MemBrokerPtr;
}  // namespace co

