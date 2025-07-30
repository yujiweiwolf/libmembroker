// Copyright 2025 Fancapital Inc.  All rights reserved.
#pragma once
#include <string>
#include <vector>
#include <memory>
#include "risk_options.h"

namespace co {
class RiskMaster {
 public:
    RiskMaster();
    ~RiskMaster();

    void Init(const std::vector<std::shared_ptr<RiskOptions>>& opts);

    void Start();

    void HandleTradeOrderReq(MemTradeOrderMessage* req, std::string* error);

    void HandleTradeOrderRep(MemTradeOrderMessage* rep);

    void HandleTradeWithdrawReq(MemTradeWithdrawMessage* req, std::string* error);

    void HandleTradeWithdrawRep(MemTradeWithdrawMessage* rep);

    void OnTradeKnock(MemTradeKnock* knock);

    void OnTick(MemQTickBody* tick);

 private:
    class RiskMasterImpl;
    RiskMasterImpl* m_ = nullptr;
};
typedef std::shared_ptr<RiskMaster> RiskMasterPtr;
}  // namespace co
