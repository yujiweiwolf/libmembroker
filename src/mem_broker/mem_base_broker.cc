// Copyright 2021 Fancapital Inc.  All rights reserved.
#include "mem_base_broker.h"
#include "mem_server.h"

namespace co {
    MemBroker::MemBroker() {
    }

    MemBroker::~MemBroker() {
    }

    void MemBroker::Init(const MemBrokerOptions& opt, MemBrokerServer* server, x::MMapWriter* rep_writer) {
        try {
            LOG_INFO << "initialize broker ...";
            server_ = server;
            rep_writer_ = rep_writer;
            enable_stock_short_selling_ = opt.enable_stock_short_selling();
            request_timeout_ms_ = opt.request_timeout_ms();
            OnInit();
            LOG_INFO << "initialize broker ok";
        } catch (std::exception& e) {
            LOG_INFO << "initialize broker failed: " << e.what();
            throw e;
        }
    }

    MemTradeAccount* MemBroker::GetAccount(const string& fund_id) {
        MemTradeAccount* ret = nullptr;
        auto itr = accounts_.find(fund_id);
        if (itr != accounts_.end()) {
            ret = &itr->second;
        } else {
            std::stringstream ss;
            ss << "[FAN-BRO-AccountError] no such account: fund_id = " << fund_id;
            throw std::runtime_error(ss.str());
        }
        return ret;
    }

    const std::map<string, MemTradeAccount>& MemBroker::GetAccounts() const {
        return accounts_;
    }

    bool MemBroker::ExitAccout(const string& fund_id) {
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

    void MemBroker::OnStart() {
        server_->OnStart();
    }

    void MemBroker::SendQueryTradeAsset(MemGetTradeAssetMessage* req) {
        server_->BeginTask();
        try {
            std::string fund_id = req->fund_id;
            GetAccount(fund_id);
            CheckTimeout(req->timestamp, request_timeout_ms_);
            OnQueryTradeAsset(req);
        } catch (std::exception & e) {
            std::string error = string(req->fund_id) + " query asset faild, " + string(e.what());
            int length = sizeof(MemMonitorRiskMessage);
            void* buffer = rep_writer_->OpenFrame(length);
            memcpy(buffer, (void*)req, length);
            MemMonitorRiskMessage* rep = (MemMonitorRiskMessage*)buffer;
            strncpy(rep->error, error.c_str(), kMemErrorSize - 1);
            rep_writer_->CloseFrame(kMemTypeMonitorRisk);
        }
        server_->EndTask();
    }

    void MemBroker::SendQueryTradePosition(MemGetTradePositionMessage* req) {
        server_->BeginTask();
        try {
            std::string fund_id = req->fund_id;
            GetAccount(fund_id);
            CheckTimeout(req->timestamp, request_timeout_ms_);
            OnQueryTradePosition(req);
        } catch (std::exception & e) {
            std::string error = string(req->fund_id) + " query pos faild, " + string(e.what());
            int length = sizeof(MemMonitorRiskMessage);
            void* buffer = rep_writer_->OpenFrame(length);
            memcpy(buffer, (void*)req, length);
            MemMonitorRiskMessage* rep = (MemMonitorRiskMessage*)buffer;
            strncpy(rep->error, error.c_str(), kMemErrorSize - 1);
            rep_writer_->CloseFrame(kMemTypeMonitorRisk);
        }
        server_->EndTask();
    }

    void MemBroker::SetInitPositions(MemGetTradePositionMessage* rep, int8_t dtype) {
        if (dtype == co::kTradeTypeSpot) {
            inner_option_master_.SetInitPositions(rep);
        } else if (dtype == co::kTradeTypeOption) {
            inner_stock_master_.SetInitPositions(rep);
        }
    }

    void MemBroker::SendQueryTradeKnock(MemGetTradeKnockMessage* req) {
        server_->BeginTask();
        try {
            std::string fund_id = req->fund_id;
            GetAccount(fund_id);
            CheckTimeout(req->timestamp, request_timeout_ms_);
            OnQueryTradeKnock(req);
        } catch (std::exception & e) {
            std::string error = string(req->fund_id) + " query pos knock, " + string(e.what());
            int length = sizeof(MemMonitorRiskMessage);
            void* buffer = rep_writer_->OpenFrame(length);
            memcpy(buffer, (void*)req, length);
            MemMonitorRiskMessage* rep = (MemMonitorRiskMessage*)buffer;
            strncpy(rep->error, error.c_str(), kMemErrorSize - 1);
            rep_writer_->CloseFrame(kMemTypeMonitorRisk);
        }
        server_->EndTask();
    }

    void MemBroker::SendTradeOrder(MemTradeOrderMessage* req) {
        // 底层需返回：InnerTradeOrdersPtr
        // InnerTradeOrders.error: 如果error不为空，表示所有报单全部失败，单个委托的废单原因请填写在每个order中
        // InnerTradeOrders.batch_no: 批次号
        // 单个TradeOrder有效字段：msg, order_no, oc_flag  如果msg不为空，表示报单失败，msg为废单原因
        server_->BeginTask();
        bool is_batch = false;
        int64_t trade_type = 0;
        try {
            int items_size = req->items_size;
            if (items_size <= 0) {
                throw std::runtime_error("no orders in request");
            } else if (items_size > 1000) {
                throw std::runtime_error("too many orders in request: " + std::to_string(items_size) + ", limit is 1000");
            }
            is_batch = items_size > 1 ? true : false;
            auto account = GetAccount(req->fund_id);
            trade_type = account->type;
            if (trade_type == kTradeTypeOption && is_batch) {  // 期权暂不支持批量报单，因为自动开平仓逻辑需要支持才行
                throw std::runtime_error("option batch order is not supported yet!");
            }

            MemTradeOrder* first = (MemTradeOrder*)((char*)req + sizeof(MemTradeOrderMessage));
            for (int i = 0; i < items_size; ++i) {
                MemTradeOrder* order = first + i;
                if (strlen(order->code) == 0) {
                    throw std::runtime_error("code is required");
                }
                int64_t market = co::CodeToMarket(order->code);
                if (market <= 0) {
                    std::stringstream ss;
                    ss << "unknown market suffix of code: " << order->code;
                    throw std::runtime_error(ss.str());
                }
                int64_t oc_flag = order->oc_flag;
                if (trade_type == kTradeTypeSpot && enable_stock_short_selling_) {
                    // 处理信用账户自动融券逻辑
                    order->oc_flag = inner_stock_master_.GetOcFlag(req->fund_id, req->bs_flag, *order);
                    inner_stock_master_.HandleOrderReq(req->fund_id, req->bs_flag, *order);
                } else if (trade_type == kTradeTypeOption) {
                    // 处理期权自动开平仓逻辑：获取自动开平仓标记
                    if (oc_flag == kOcFlagAuto) {
                        order->oc_flag = inner_option_master_.GetAutoOcFlag(req->fund_id, req->bs_flag, *order);
                    }
                    inner_option_master_.HandleOrderReq(req->fund_id, req->bs_flag, *order);
                }
            }
            CheckTimeout(req->timestamp, request_timeout_ms_);
            OnTradeOrder(req);
        } catch (std::exception & e) {
            std::string error = e.what();
            if (error.empty()) {
                error = "EmptyError";
            }
            int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * req->items_size;
            void* buffer = rep_writer_->OpenFrame(length);
            MemTradeOrderMessage* rep = (MemTradeOrderMessage*)buffer;
            memcpy(rep, req, length);
            strncpy(rep->error, error.c_str(), kMemErrorSize - 1);
            // rep->rep_time = x::RawDateTime();
            rep_writer_->CloseFrame(kMemTypeTradeOrderRep);
        }
        server_->EndTask();
    }

    void MemBroker::SendTradeWithdraw(MemTradeWithdrawMessage* req) {
        server_->BeginTask();
        try {
            if (strlen(req->order_no) == 0 && strlen(req->batch_no) == 0) {
                throw std::runtime_error("order_no or batch_no is required");
            }
            OnTradeWithdraw(req);
        } catch (std::exception & e) {
            std::string error = e.what();
            if (error.empty()) {
                error = "EmptyError";
            }
            int length = sizeof(MemTradeWithdrawMessage);
            void* buffer = rep_writer_->OpenFrame(length);
            MemTradeWithdrawMessage* rep = (MemTradeWithdrawMessage*)buffer;
            memcpy(rep, req, length);
            strncpy(rep->error, error.c_str(), kMemErrorSize - 1);
            // rep->rep_time = x::RawDateTime();
            rep_writer_->CloseFrame(kMemTypeTradeWithdrawRep);
        }
        server_->EndTask();
    }

    void MemBroker::SendRtnMessage(const std::string& raw, int64_t type) {
        server_->SendRtnMessage(raw, type);
    }

    void MemBroker::SendTradeKnock(MemTradeKnock* knock) {
        auto itr = accounts_.find(knock->fund_id);
        if (itr != accounts_.end()) {
            auto accout = &itr->second;
            if (accout->type == kTradeTypeSpot && enable_stock_short_selling_) {
                inner_stock_master_.HandleKnock(knock);
            } else if (accout->type == kTradeTypeOption) {
                inner_option_master_.HandleKnock(knock);
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
            ss << "[FAN-BRO-TimeoutError] request is timeout for " << delay << "ms: now = " << now << ", timestamp = " << request_time << ", limit = " << ttl_ms << "ms";
            throw runtime_error(ss.str());
        }
        return delay;
    }

}  // namespace co

