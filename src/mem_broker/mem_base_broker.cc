// Copyright 2021 Fancapital Inc.  All rights reserved.
#include "mem_base_broker.h"
#include "mem_server.h"

namespace co {
MemBroker::MemBroker() {
}

MemBroker::~MemBroker() {
}

void MemBroker::Init(const MemBrokerOptions& opt, MemBrokerServer* server) {
    try {
        LOG_INFO << "initialize broker ...";
        server_ = server;
        enable_stock_short_selling_ = opt.enable_stock_short_selling();
        request_timeout_ms_ = opt.request_timeout_ms();
        OnInit();
        LOG_INFO << "initialize broker ok";
    } catch (std::exception& e) {
        LOG_INFO << "initialize broker failed: " << e.what();
    }
}

MemTradeAccount* MemBroker::GetAccount(const string& fund_id) {
    MemTradeAccount* ret = nullptr;
    auto itr = accounts_.find(fund_id);
    if (itr != accounts_.end()) {
        ret = &itr->second;
    } else {
        std::stringstream ss;
        ss << "[FAN-BROKER-ERROR] no such account: fund_id = " << fund_id;
        throw std::runtime_error(ss.str());
    }
    return ret;
}

const std::map<string, MemTradeAccount>& MemBroker::GetAccounts() const {
    return accounts_;
}

bool MemBroker::ExitAccount(const string& fund_id) {
    auto itr = accounts_.find(fund_id);
    if (itr != accounts_.end()) {
        return true;
    } else {
        return false;
    }
}

void MemBroker::AddAccount(const co::MemTradeAccount& account) {
    if (account.type != kTradeTypeSpot &&
        account.type != kTradeTypeFuture &&
        account.type != kTradeTypeOption) {
        LOG_ERROR << ("illegal trade_type: " + to_string(account.type));
    }
    if (strlen(account.fund_id) == 0) {
        LOG_ERROR << ("illegal fund_id: " +  string(account.fund_id));
    }
    LOG_INFO << "Add account: " << account.fund_id << ", type: " << account.type;
    accounts_[account.fund_id] = account;
}

void MemBroker::AddT0Code(const string& code) {
    inner_stock_master_.AddT0Code(code);
}

void MemBroker::OnStart() {
    server_->OnStart();
}

void MemBroker::SendQueryTradeAsset(MemGetTradeAssetMessage* req) {
    try {
        CheckTimeout(req->timestamp, request_timeout_ms_);
        OnQueryTradeAsset(req);
    } catch (std::exception & e) {
        std::string error = string(req->fund_id) + " query asset failed, " + string(e.what());
        MemMonitorRiskMessage msg = {};
        msg.timestamp = x::RawDateTime();
        memcpy(msg.error, error.c_str(), sizeof(msg.error));
        SendRtnMessage(string(reinterpret_cast<char*>(&msg), sizeof(msg)), kMemTypeMonitorRisk);
    }
}

void MemBroker::SendQueryTradePosition(MemGetTradePositionMessage* req) {
    try {
        CheckTimeout(req->timestamp, request_timeout_ms_);
        OnQueryTradePosition(req);
    } catch (std::exception & e) {
        std::string error = string(req->fund_id) + " query pos failed, " + string(e.what());
        MemMonitorRiskMessage msg = {};
        msg.timestamp = x::RawDateTime();
        memcpy(msg.error, error.c_str(), sizeof(msg.error));
        SendRtnMessage(string(reinterpret_cast<char*>(&msg), sizeof(msg)), kMemTypeMonitorRisk);
    }
}

void MemBroker::InitPositions(MemGetTradePositionMessage* rep, int64_t type) {
    if (type == co::kTradeTypeSpot) {
        inner_option_master_.InitPositions(rep);
    } else if (type == co::kTradeTypeOption) {
        inner_stock_master_.InitPositions(rep);
    } else if (type == co::kTradeTypeFuture) {
        inner_future_master_.InitPositions(rep);
    }
}

void MemBroker::SendQueryTradeKnock(MemGetTradeKnockMessage* req) {
    try {
        CheckTimeout(req->timestamp, request_timeout_ms_);
        OnQueryTradeKnock(req);
    } catch (std::exception & e) {
        std::string error = string(req->fund_id) + " query knock failed, " + string(e.what());
        MemMonitorRiskMessage msg = {};
        msg.timestamp = x::RawDateTime();
        memcpy(msg.error, error.c_str(), sizeof(msg.error));
        SendRtnMessage(string(reinterpret_cast<char*>(&msg), sizeof(msg)), kMemTypeMonitorRisk);
    }
}

void MemBroker::SendTradeOrder(MemTradeOrderMessage* req) {
    try {
        auto account = GetAccount(req->fund_id);
        int64_t trade_type = account->type;
        MemTradeOrder* first = (MemTradeOrder*)((char*)req + sizeof(MemTradeOrderMessage));
        for (int i = 0; i < req->items_size; ++i) {
            MemTradeOrder* order = req->items + i;
            int64_t oc_flag = order->oc_flag;
            LOG_INFO << req->fund_id << ", trade_type: " << account->type << ", oc_flag: " << oc_flag;
            if (trade_type == kTradeTypeSpot && enable_stock_short_selling_) {
                // 处理信用账户自动融券逻辑
                order->oc_flag = inner_stock_master_.GetAutoOcFlag(req->bs_flag, *order);
                inner_stock_master_.HandleOrderReq(req->bs_flag, *order);
            } else if (trade_type == kTradeTypeOption) {
                // 处理期权自动开平仓逻辑：获取自动开平仓标记
                if (oc_flag == kOcFlagAuto) {
                    order->oc_flag = inner_option_master_.GetAutoOcFlag(req->bs_flag, *order);
                }
                inner_option_master_.HandleOrderReq(req->bs_flag, *order);
            } else if (trade_type == kTradeTypeFuture) {
                // 处理期货自动开平仓逻辑：获取自动开平仓标记, 平仓可能变成开仓
                order->oc_flag = inner_future_master_.GetAutoOcFlag(req->bs_flag, *order);
                inner_future_master_.HandleOrderReq(req->bs_flag, *order);
            }
        }
        OnTradeOrder(req);
    } catch (std::exception & e) {
        std::string error = e.what();
        if (error.empty()) {
            error = "[FAN-BROKER-ERROR] EmptyError";
        }
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * req->items_size;
        char buffer[length] = "";
        memcpy(buffer, req, length);
        MemTradeOrderMessage* rep = (MemTradeOrderMessage*)buffer;
        memcpy(rep->error, error.c_str(), sizeof(rep->error));
        SendRtnMessage(string(buffer, length), kMemTypeTradeOrderRep);
    }
}

void MemBroker::SendTradeWithdraw(MemTradeWithdrawMessage* req) {
    try {
        OnTradeWithdraw(req);
    } catch (std::exception & e) {
        std::string error = e.what();
        if (error.empty()) {
            error = "[FAN-BROKER-ERROR] EmptyError";
        }
        int length = sizeof(MemTradeWithdrawMessage);
        char buffer[sizeof(MemTradeWithdrawMessage)] = "";
        memcpy(buffer, req, length);
        MemTradeWithdrawMessage* rep = (MemTradeWithdrawMessage*)buffer;
        memcpy(rep->error, error.c_str(), sizeof(rep->error));
        SendRtnMessage(string(buffer, length), kMemTypeTradeWithdrawRep);
    }
}

void MemBroker::SendRtnMessage(const std::string& raw, int64_t type) {
    server_->SendRtnMessage(raw, type);
}

void MemBroker::HandleTradeOrderRep(MemTradeOrderMessage* rep) {
    auto itr = accounts_.find(rep->fund_id);
    if (itr != accounts_.end()) {
        auto account = &itr->second;
        if (account->type == kTradeTypeSpot && enable_stock_short_selling_) {
            MemTradeOrder* items = rep->items;
            for (int i = 0; i < rep->items_size; i++) {
                MemTradeOrder* order = items + i;
                inner_stock_master_.HandleOrderRep(rep->bs_flag, *order);
            }
        } else if (account->type == kTradeTypeOption) {
            MemTradeOrder* items = rep->items;
            for (int i = 0; i < rep->items_size; i++) {
                MemTradeOrder* order = items + i;
                inner_option_master_.HandleOrderRep(rep->bs_flag, *order);
            }
        } else if (account->type == kTradeTypeFuture) {
            MemTradeOrder* items = rep->items;
            for (int i = 0; i < rep->items_size; i++) {
                MemTradeOrder* order = items + i;
                inner_future_master_.HandleOrderRep(rep->bs_flag, *order);
            }
        }
    }
}

void MemBroker::HandleTradeKnock(MemTradeKnock* knock) {
    auto itr = accounts_.find(knock->fund_id);
    if (itr != accounts_.end()) {
        auto account = &itr->second;
        if (account->type == kTradeTypeSpot && enable_stock_short_selling_) {
            inner_stock_master_.HandleKnock(*knock);
        } else if (account->type == kTradeTypeOption) {
            inner_option_master_.HandleKnock(*knock);
        } else if (account->type == kTradeTypeFuture) {
            inner_future_master_.HandleKnock(*knock);
        }
    }
}

void MemBroker::OnInit() {
    // pass
}

void MemBroker::OnQueryTradeAsset(MemGetTradeAssetMessage* req) {
    throw std::runtime_error("BrokerUnimplementedError");
}

void MemBroker::OnQueryTradePosition(MemGetTradePositionMessage* req) {
    throw std::runtime_error("BrokerUnimplementedError");
}

void MemBroker::OnQueryTradeKnock(MemGetTradeKnockMessage* req) {
    throw std::runtime_error("BrokerUnimplementedError");
}

void MemBroker::OnTradeOrder(MemTradeOrderMessage* req) {
    throw std::runtime_error("BrokerUnimplementedError");
}

void MemBroker::OnTradeWithdraw(MemTradeWithdrawMessage* req) {
    throw std::runtime_error("BrokerUnimplementedError");
}

int64_t MemBroker::CheckTimeout(int64_t request_time, int64_t ttl_ms) {
    int64_t now = x::RawDateTime();
    int64_t delay = x::SubRawDateTime(now, request_time);
    if (ttl_ms > 0 && (delay > ttl_ms || delay < -ttl_ms)) {
        stringstream ss;
        ss << "[FAN-BROKER-ERROR] request is timeout for " << delay << "ms: now = " << now << ", timestamp = " << request_time << ", limit = " << ttl_ms << "ms";
        throw runtime_error(ss.str());
    }
    return delay;
}
}  // namespace co

