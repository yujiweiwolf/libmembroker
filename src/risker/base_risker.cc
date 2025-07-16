// Copyright 2021 Fancapital Inc.  All rights reserved.
#include "../../src/risker/base_risker.h"

namespace co {
void Risker::Init(std::shared_ptr<RiskOptions> opt) {}

std::string Risker::HandleTradeOrderReq(MemTradeOrderMessage* req) {
    return "";
}

void Risker::OnTradeOrderReqPass(MemTradeOrderMessage* req) {}

void Risker::HandleTradeOrderRep(MemTradeOrderMessage* rep) {}

std::string Risker::HandleTradeWithdrawReq(MemTradeWithdrawMessage* req) {
    return "";
}

void Risker::OnTradeWithdrawReqPass(MemTradeWithdrawMessage* req) {}

void Risker::HandleTradeWithdrawRep(MemTradeWithdrawMessage* rep) {}

void Risker::OnTradeKnock(MemTradeKnock* knock) {}
}  // namespace co
