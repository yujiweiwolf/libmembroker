// Copyright 2025 Fancapital Inc.  All rights reserved.
#include <utility>
#include <functional>
#include "yaml-cpp/yaml.h"
#include "fancapital_risker.h"
#include "../common/order_book.h"

namespace co {

    void FancapitalRisker::Init(std::shared_ptr<RiskOptions> opt) {
        LOG_INFO << "[risk][fancapital] init ok: options = " << opt->data();
    }
//
//    void FancapitalRisker::HandleTradeOrderReq(MemTradeOrderMessage* req, std::string* error) {
//
//    }
//
//    void FancapitalRisker::HandleTradeOrderRep(const co::fbs::TradeOrderMessage* rep) {
//
//    }
//
//    void FancapitalRisker::HandleTradeWithdrawReq(MemTradeWithdrawMessage* req, std::string* error) {
//
//    }
//
//    void FancapitalRisker::HandleTradeWithdrawRep(const co::fbs::TradeWithdrawMessage* rep) {
//
//    }
//
//    void FancapitalRisker::OnTradeKnock(const co::fbs::TradeKnock* knock) {
//
//    }

}   // namespace co