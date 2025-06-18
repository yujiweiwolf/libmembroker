// Copyright 2021 Fancapital Inc.  All rights reserved.
#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <regex>
#include <utility>
#include <boost/filesystem.hpp>
#include <boost/algorithm/hex.hpp>
#include "mem_server.h"

namespace co {
MemBrokerServer::MemBrokerServer() {
    start_time_ = x::RawDateTime();
    queue_ = std::make_shared<BrokerQueue>();
    flow_control_queue_ = std::make_shared<FlowControlQueue>(queue_.get());
    risker_ = std::make_shared<RiskMaster>();
}

MemBrokerServer::~MemBrokerServer() {
}

void MemBrokerServer::Init(MemBrokerOptionsPtr option, const std::vector<std::shared_ptr<RiskOptions>>& risk_opts, MemBrokerPtr broker) {
    opt_ = option;
    broker_ = broker;
    risker_->Init(risk_opts);
    enable_flow_control_ = opt_->IsFlowControlEnabled();
    if (enable_flow_control_) {
        flow_control_queue_->Init(opt_);
    }
}

void  MemBrokerServer::Start() {
    threads_.emplace_back(std::make_shared<std::thread>(std::bind(& MemBrokerServer::Run, this)));
}

void  MemBrokerServer::Join() {
    for (auto& thread : threads_) {
        thread->join();
    }
}

void MemBrokerServer::Run() {
    LOG_INFO << "start broker server ...";
    if (!opt_) {
        throw std::runtime_error("options is required, please initialize broker server before starting");
    }
    if (!broker_) {
        throw std::runtime_error("broker is required, please initialize broker server before starting");
    }

    rep_writer_.Open(opt_->mem_dir(), opt_->mem_rep_file(), kRepMemSize << 20, true);
    broker_->Init(*opt_, this, &rep_writer_);
    LoadTradingData();

    auto accounts = broker_->GetAccounts();
    string fc_spot_fund_id;
    for (auto itr = accounts.begin(); itr != accounts.end(); ++itr) {
        auto& acc = itr->second;
        if (enable_flow_control_ && acc.type == kTradeTypeSpot) {
            fc_spot_fund_id = acc.fund_id;
        }
    }
    if (enable_flow_control_) {
        flow_control_queue_->InitState(fc_spot_fund_id);
    }

    threads_.emplace_back(std::make_shared<std::thread>(std::bind(& MemBrokerServer::RunQuery, this)));
    threads_.emplace_back(std::make_shared<std::thread>(std::bind(& MemBrokerServer::RunWatch, this)));
    threads_.emplace_back(std::make_shared<std::thread>(std::bind(& MemBrokerServer::ReadReqMem, this)));
    HandleQueueMessage();
}

void MemBrokerServer::LoadTradingData() {
    LOG_INFO << "load trading data ...";
    auto t1 = x::Timestamp();
    x::MMapReader rep_reader;
    rep_reader.Open(opt_->mem_dir(), opt_->mem_rep_file(), false);
    const void* data = nullptr;
    while (true) {
        int32_t type = rep_reader.Next(&data);
        if (type == kMemTypeTradeKnock) {
            MemTradeKnock* knock = (MemTradeKnock*) data;
            IsNewMemTradeKnock(knock);
        } else if (type == kMemTypeQueryTradeAssetRep) {
            MemGetTradeAssetMessage* rep = (MemGetTradeAssetMessage*) data;
            auto item = (MemTradeAsset*)((char*)rep + sizeof(MemGetTradeAssetMessage));
            for (int i = 0; i < rep->items_size; i++) {
                MemTradeAsset *asset = item + i;
                if (strlen(asset->fund_id) > 0) {
                    assets_[asset->fund_id] = *asset;
                }
            }
        } else if (type == kMemTypeQueryTradePositionRep) {
            MemGetTradePositionMessage* rep = (MemGetTradePositionMessage*) data;
            auto item = (MemTradePosition*)((char*)rep + sizeof(MemGetTradePositionMessage));
            for (int i = 0; i < rep->items_size; i++) {
                MemTradePosition *position = item + i;
                if (strlen(position->fund_id) > 0 && strlen(position->code) > 0) {
                    std::shared_ptr<std::map<std::string, MemTradePosition>> recode;
                    auto itor = positions_.find(position->fund_id);
                    if (itor == positions_.end()) {
                        recode = std::make_shared<std::map<std::string, MemTradePosition>>();
                        recode->insert(std::make_pair(position->code, *position));
                        positions_.insert(std::make_pair(position->fund_id, recode));
                    } else {
                        (*recode)[position->code] = *position;
                    }
                }
            }
        } else if (type == 0) {
            break;
        }
    }
    size_t position_rows = 0;
    for (auto& itr : positions_) {
        auto all = itr.second;
        position_rows += all->size();
    }
    auto t2 = x::Timestamp();
    LOG_INFO << "load trading data ok in " << (t2 - t1)
             << "ms, asset: " << assets_.size()
             << ", position: " << position_rows
             << ", knock: " << knocks_.size();
 }

bool MemBrokerServer::ExitAccount(const string& fund_id) {
     return broker_->ExitAccount(fund_id);
 }

void MemBrokerServer::BeginTask() {
    active_task_timestamp_ = x::Timestamp();
}

void  MemBrokerServer::EndTask() {
    active_task_timestamp_ = 0;
}

void MemBrokerServer::OnStart() {
    const auto& accounts = broker_->GetAccounts();
    std::vector<std::string> stock_fund_ids;
    std::vector<std::string> option_fund_ids;
    for (auto& itr : accounts) {
        auto acc = itr.second;
        if (acc.type == kTradeTypeSpot) {
            stock_fund_ids.push_back(acc.fund_id);
        } else if (acc.type == kTradeTypeOption) {
            option_fund_ids.push_back(acc.fund_id);
        }
    }

    if (opt_->enable_stock_short_selling() && !stock_fund_ids.empty()) {
        for (auto& it : stock_fund_ids) {
            std::string& fund_id = it;
            LOG_INFO << "query stock init position: fund_id = " << fund_id << " ...";
            char buffer[sizeof(MemGetTradePositionMessage)] = "";
            MemGetTradePositionMessage* req = (MemGetTradePositionMessage*)buffer;
            string id = "INIT_STOCK_" + x::UUID();
            req->timestamp = x::RawDateTime();
            strncpy(req->id, id.c_str(), id.length());
            strncpy(req->fund_id, fund_id.c_str(), fund_id.length());
            queue_->Push(nullptr, kMemTypeQueryTradePositionReq,
                         string(static_cast<const char*>(buffer), sizeof(MemGetTradePositionMessage)));
        }
    }

    if (!option_fund_ids.empty()) {
        for (auto& it : option_fund_ids) {
            std::string& fund_id = it;
            LOG_INFO << "query option init position: fund_id = " << fund_id << " ...";
            char buffer[sizeof(MemGetTradePositionMessage)] = "";
            MemGetTradePositionMessage* req = (MemGetTradePositionMessage*)buffer;
            string id = "INIT_OPTION_" + x::UUID();
            req->timestamp = x::RawDateTime();
            strncpy(req->id, id.c_str(), id.length());
            strncpy(req->fund_id, fund_id.c_str(), fund_id.length());
            queue_->Push(nullptr, kMemTypeQueryTradePositionReq,
                         string(static_cast<const char*>(buffer), sizeof(MemGetTradePositionMessage)));
        }
    }
 }

void MemBrokerServer::ReadReqMem() {
    string mem_dir = opt_->mem_dir();
    string mem_req_file = opt_->mem_req_file();
    string mem_rep_file = opt_->mem_rep_file();

    bool exit_flag = false;
    if (boost::filesystem::exists(mem_dir)) {
        boost::filesystem::path p(mem_dir);
        for (auto &file : boost::filesystem::directory_iterator(p)) {
            const string filename = file.path().filename().string();
            if (filename.find(mem_req_file) != filename.npos) {
                exit_flag = true;
                break;
            }
        }
    }
    if (!exit_flag) {
        x::MMapWriter req_writer;
        req_writer.Open(mem_dir, mem_req_file, kReqMemSize << 20, true);
        req_writer.Close();
    }
    x::MMapReader consume_reader;  // 抢占式读网关的报撤单数据
    consume_reader.SetEnableConsume(true);
    consume_reader.Open(mem_dir, mem_req_file, true);

    const void* data = nullptr;
    auto get_req = [&](int32_t type, const void* data)-> bool {
        if (type == kMemTypeTradeOrderReq) {
            MemTradeOrderMessage *msg = (MemTradeOrderMessage*)data;
            if (ExitAccount(msg->fund_id)) {
                return true;
            } else {
                return false;
            }
        } else if (type == kMemTypeTradeWithdrawReq) {
            MemTradeWithdrawMessage *msg = (MemTradeWithdrawMessage *)data;
            if (ExitAccount(msg->fund_id)) {
                return true;
            } else {
                return false;
            }
        }
        return false;
    };
    while (true) {
        // 抢占式读网关的报撤单数据, 先过风控，再过流控; 本broker的帐号事前风控，其它帐号从rep中读取信息，事后风控
        while (true) {
            int32_t type = consume_reader.ConsumeWhere(&data, get_req, true);
            if (type == kMemTypeTradeOrderReq) {
                MemTradeOrderMessage *req = (MemTradeOrderMessage*) data;
                int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * req->items_size;
                string error;
                risker_->HandleTradeOrderReq(req, &error);
                if (!error.empty()) {
                    char buffer[length] = {0};
                    memcpy(buffer, req, length);
                    MemTradeOrderMessage *rep = (MemTradeOrderMessage*)buffer;
                    strncpy(rep->error, error.c_str(), error.length());
                    queue_->Push(nullptr, kMemTypeTradeOrderRep, string(buffer, length));
                } else {
                    queue_->Push(nullptr, kMemTypeTradeOrderReq, string(reinterpret_cast<const char*>(data), length));
                }
            } else if (type == kMemTypeTradeWithdrawReq) {
                MemTradeWithdrawMessage *req = (MemTradeWithdrawMessage*) data;
                int length = sizeof(MemTradeWithdrawMessage);
                string error;
                risker_->HandleTradeWithdrawReq(req, &error);
                if (!error.empty()) {
                    char buffer[length] = {0};
                    memcpy(buffer, req, length);
                    MemTradeWithdrawMessage *rep = (MemTradeWithdrawMessage*)buffer;
                    strncpy(rep->error, error.c_str(), error.length());
                    queue_->Push(nullptr, kMemTypeTradeWithdrawRep, string(buffer, length));
                } else {
                    queue_->Push(nullptr, kMemTypeTradeWithdrawReq, string(reinterpret_cast<const char*>(data), length));
                }
            } else {
                break;
            }
        }
    }
 }

void MemBrokerServer::HandleQueueMessage() {
    try {
        int cpu_affinity = opt_->cpu_affinity();
        if (cpu_affinity > 0) {
            x::SetCPUAffinity(cpu_affinity);
        }
        while (true) {
            BrokerMsg* msg = enable_flow_control_ ? flow_control_queue_->Pop() : queue_->Pop();
            int64_t queue_size = enable_flow_control_ ? flow_control_queue_->GetNormalQueueSize() : queue_->Size();
            int64_t fc_size = 0;
            int64_t fc_total_size = 0;
            flow_control_queue_->GetFlowControlQueueSize(&fc_size, &fc_total_size);
            int64_t function_id = msg->function_id();
            std::string raw = msg->data();
            BrokerMsg::Destory(msg);
            if (function_id > 0 && raw.length() > 0) {
                switch (function_id) {
                    case kMemTypeTradeOrderReq: {
                        MemTradeOrderMessage *msg = reinterpret_cast<MemTradeOrderMessage*>(raw.data());
                        SendTradeOrder(msg);
                        break;
                    }
                    case kMemTypeTradeWithdrawReq: {
                        MemTradeWithdrawMessage *msg = reinterpret_cast<MemTradeWithdrawMessage*>(raw.data());
                        SendTradeWithdraw(msg);
                        break;
                    }
                    case kMemTypeQueryTradeAssetReq: {
                        MemGetTradeAssetMessage *msg = reinterpret_cast<MemGetTradeAssetMessage*>(raw.data());
                        SendQueryTradeAsset(msg);
                        break;
                    }
                    case kMemTypeQueryTradePositionReq: {
                        MemGetTradePositionMessage *msg = reinterpret_cast<MemGetTradePositionMessage*>(raw.data());
                        SendQueryTradePosition(msg);
                        break;
                    }
                    case kMemTypeQueryTradeKnockReq: {
                        MemGetTradeKnockMessage *msg = reinterpret_cast<MemGetTradeKnockMessage*>(raw.data());
                        SendQueryTradeKnock(msg);
                        break;
                    }
                    case kMemTypeTradeOrderRep: {
                        MemTradeOrderMessage *msg = reinterpret_cast<MemTradeOrderMessage*>(raw.data());
                        risker_->HandleTradeOrderRep(msg);
                        SendTradeOrderRep(msg);
                        break;
                    }
                    case kMemTypeTradeWithdrawRep: {
                        MemTradeWithdrawMessage *msg = reinterpret_cast<MemTradeWithdrawMessage*>(raw.data());
                        risker_->HandleTradeWithdrawRep(msg);
                        SendTradeWithdrawRep(msg);
                        break;
                    }
                    case kMemTypeTradeKnock: {
                        MemTradeKnock *msg = reinterpret_cast<MemTradeKnock*>(raw.data());
                        risker_->OnTradeKnock(msg);
                        SendTradeKnock(msg);
                        break;
                    }
                    case kMemTypeQueryTradeAssetRep: {
                        MemGetTradeAssetMessage *msg = reinterpret_cast<MemGetTradeAssetMessage*>(raw.data());
                        SendQueryTradeAssetRep(msg);
                        break;
                    }
                    case kMemTypeQueryTradePositionRep: {
                        MemGetTradePositionMessage *msg = reinterpret_cast<MemGetTradePositionMessage*>(raw.data());
                        SendQueryTradePositionRep(msg);
                        break;
                    }
                    case kMemTypeQueryTradeKnockRep: {
                        MemGetTradeKnockMessage *msg = reinterpret_cast<MemGetTradeKnockMessage*>(raw.data());
                        SendQueryTradeKnockRep(msg);
                        break;
                    }
                    case kMemTypeInnerCyclicSignal: {
                        DoWatch();
                        break;
                    }
                    default:
                        LOG_ERROR << "handle message failed: unknown function_id: " << function_id;
                        break;
                }
            }
        }
    } catch (std::exception & e) {
        LOG_ERROR << "handle message error: " << e.what();
    }
 }

void MemBrokerServer::RunQuery() {
    flatbuffers::FlatBufferBuilder fbb;
    int64_t query_asset_ms = opt_->query_asset_interval_ms();
    int64_t query_position_ms = opt_->query_position_interval_ms();
    int64_t query_knock_ms = opt_->query_knock_interval_ms();
    auto& accounts = broker_->GetAccounts();
    for (auto& itr : accounts) {
        auto acc = itr.second;
        std::string fund_id = acc.fund_id;
        std::string fund_name = acc.name;
        if (query_asset_ms > 0) {
            auto ctx = new QueryContext();
            ctx->set_fund_id(fund_id);
            ctx->set_fund_name(fund_name);
            ctx->set_last_success_time(x::Timestamp());
            asset_contexts_[fund_id] = ctx;
        }
        if (query_position_ms > 0) {
            auto ctx = new QueryContext();
            ctx->set_fund_id(fund_id);
            ctx->set_fund_name(fund_name);
            ctx->set_last_success_time(x::Timestamp());
            position_contexts_[fund_id] = ctx;
        }
        if (query_knock_ms > 0) {
            auto ctx = new QueryContext();
            ctx->set_fund_id(fund_id);
            ctx->set_fund_name(fund_name);
            ctx->set_last_success_time(x::Timestamp());
            knock_contexts_[fund_id] = ctx;
        }
    }
    if (asset_contexts_.empty() && position_contexts_.empty() && knock_contexts_.empty()) {
        return;
    }
    int64_t asset_timeout_ms = query_asset_ms < 5000 ? 5000 : query_asset_ms;
    int64_t position_timeout_ms = query_position_ms < 10000 ? 10000 : query_position_ms;
    int64_t knock_timeout_ms = query_knock_ms < 10000 ? 10000 : query_knock_ms;
    int64_t start_time = x::Timestamp();
    while (true) {
        x::Sleep(100);
        int64_t now = x::Timestamp();
        for (auto& itr : asset_contexts_) {
            auto ctx = itr.second;
            int64_t max_time = ctx->rep_time() >= ctx->req_time() ? ctx->rep_time() : ctx->req_time();
            bool active = ctx->rep_time() < ctx->req_time() ? true : false;
            if ((!active && now - max_time >= query_asset_ms) || now - max_time >= asset_timeout_ms) {
                ctx->set_req_time(now);
                char buffer[sizeof(MemGetTradeAssetMessage)] = "";
                MemGetTradeAssetMessage* req = (MemGetTradeAssetMessage*)buffer;
                string id = x::UUID();
                string fund_id = ctx->fund_id();
                req->timestamp = x::RawDateTime();
                strncpy(req->id, id.c_str(), id.length());
                strncpy(req->fund_id, fund_id.c_str(), fund_id.length());
                queue_->Push(nullptr, kMemTypeQueryTradeAssetReq,
                             string(static_cast<const char*>(buffer), sizeof(MemTradeWithdrawMessage)));
            }
        }

        for (auto& itr : position_contexts_) {
            auto ctx = itr.second;
            int64_t max_time = ctx->rep_time() >= ctx->req_time() ?
                    ctx->rep_time() : ctx->req_time();
            bool active = ctx->rep_time() < ctx->req_time() ? true : false;
            if ((!active && now - max_time >= query_position_ms) || now - max_time >= position_timeout_ms) {
                ctx->set_req_time(now);
                char buffer[sizeof(MemGetTradePositionMessage)] = "";
                MemGetTradePositionMessage* req = (MemGetTradePositionMessage*)buffer;
                string id = x::UUID();
                string fund_id = ctx->fund_id();
                req->timestamp = x::RawDateTime();
                strncpy(req->id, id.c_str(), id.length());
                strncpy(req->fund_id, fund_id.c_str(), fund_id.length());
                queue_->Push(nullptr, kMemTypeQueryTradePositionReq,
                             string(static_cast<const char*>(buffer), sizeof(MemGetTradePositionMessage)));
            }
        }

        for (auto& itr : knock_contexts_) {
            auto ctx = itr.second;
            std::string next_cursor = ctx->next_cursor();
            bool must_query = false;
            if (!ctx->inited() && next_cursor != ctx->cursor()) {
                must_query = true;
            }
            int64_t max_time = ctx->rep_time() >= ctx->req_time() ? ctx->rep_time() : ctx->req_time();
            bool active = ctx->rep_time() < ctx->req_time() ? true : false;
            if ((!active && now - max_time >= knock_timeout_ms) || now - max_time >= knock_timeout_ms || must_query) {
                ctx->set_req_time(now);
                ctx->set_cursor(next_cursor);
                char buffer[sizeof(MemGetTradeKnockMessage)] = "";
                MemGetTradeKnockMessage* req = (MemGetTradeKnockMessage*)buffer;
                string id = x::UUID();
                string fund_id = ctx->fund_id();
                req->timestamp = x::RawDateTime();
                strncpy(req->id, id.c_str(), id.length());
                strncpy(req->fund_id, fund_id.c_str(), fund_id.length());
                strncpy(req->cursor, next_cursor.c_str(), next_cursor.length());
                queue_->Push(nullptr, kMemTypeQueryTradeKnockReq,
                             string(static_cast<const char*>(buffer), sizeof(MemGetTradeKnockMessage)));
            }
        }
    }
}

// 请求转发给broker
void MemBrokerServer::SendQueryTradeAsset(MemGetTradeAssetMessage* req) {
    wait_size_++;
    int64_t ms = x::SubRawDateTime(x::RawDateTime(), req->timestamp);
    LOG_INFO << "[REQ][WaitRep=" << wait_size_ << "] query asset: req_delay = " << ms << "ms";
    broker_->SendQueryTradeAsset(req);
}

void MemBrokerServer::SendQueryTradePosition(MemGetTradePositionMessage* req) {
    wait_size_++;
    int64_t ms = x::SubRawDateTime(x::RawDateTime(), req->timestamp);
    LOG_INFO << "[REQ][WaitRep=" << wait_size_ << "] query position: req_delay = " << ms << "ms";
    broker_->SendQueryTradePosition(req);
}

void  MemBrokerServer::SendQueryTradeKnock(MemGetTradeKnockMessage* req) {
    wait_size_++;
    int64_t ms = x::SubRawDateTime(x::RawDateTime(), req->timestamp);
    LOG_INFO << "[REQ][WaitRep=" << wait_size_ << "] query knock: req_delay = " << ms << "ms";
    broker_->SendQueryTradeKnock(req);
}

void MemBrokerServer::SendTradeOrder(MemTradeOrderMessage* req) {
    int64_t now = x::RawDateTime();
    int64_t ms = x::SubRawDateTime(now, req->timestamp);
    pending_orders_.insert(std::make_pair(req->id, now));
    LOG_INFO << "[REQ][WaitRep=" << pending_orders_.size() << "] send order: req_delay = " << ms << "ms, req = " << ToString(req) << " ...";
    broker_->SendTradeOrder(req);
}

void MemBrokerServer::SendTradeWithdraw(MemTradeWithdrawMessage* req) {
    int64_t now = x::RawDateTime();
    int64_t ms = x::SubRawDateTime(now, req->timestamp);
    pending_withdraws_.insert(std::make_pair(req->id, now));
    LOG_INFO << "[REQ][WaitRep=" << pending_withdraws_.size() << "] send withdraw: req_delay = " << ms << "ms, req = " << ToString(req) << " ...";
    broker_->SendTradeWithdraw(req);
}

bool MemBrokerServer::MemBrokerServer::IsNewMemTradeKnock(MemTradeKnock* knock) {
    CreateInnerMatchNo(knock);
    bool flag = false;
    string key = string(knock->inner_match_no);
    auto it = knocks_.find(key);
    if (it == knocks_.end()) {
        knocks_.insert(key);
        flag = true;
    }
    return flag;
 }

void MemBrokerServer::SendQueryTradeAssetRep(MemGetTradeAssetMessage* rep) {
    int64_t ms = x::SubRawDateTime(x::RawDateTime(), rep->timestamp);
    std::string fund_id = rep->fund_id;
    std::string error = rep->error;
    QueryContext* ctx = nullptr;
    auto itr_cursor = asset_contexts_.find(fund_id);
    if (itr_cursor != asset_contexts_.end()) {
        ctx = itr_cursor->second;
    }
    if (ctx) {
        int64_t now = x::Timestamp();
        ctx->set_rep_time(now);
        if (error.empty()) {
            ctx->set_last_success_time(now);
        }
    }
    wait_size_--;
    if (!error.empty()) {
        LOG_ERROR << "[REP]query asset failed in " << ms << "ms: " << error;
        return;
    }
    auto first = (MemTradeAsset*)((char*)rep + sizeof(MemGetTradeAssetMessage));
    for (int i = 0; i < rep->items_size; i++) {
        MemTradeAsset *asset = first + i;
        bool flag = false;
        string fund_id = asset->fund_id;
        auto it = assets_.find(fund_id);
        if (it == assets_.end()) {
            flag = true;
            assets_.insert(std::make_pair(fund_id, *asset));
        } else {
            double epsilon = 0.01;
            MemTradeAsset* pre = &it->second;
            if (std::fabs(asset->balance - pre->balance) >= epsilon ||
                std::fabs(asset->usable - pre->usable) >= epsilon ||
                std::fabs(asset->margin - pre->margin) >= epsilon ||
                std::fabs(asset->equity - pre->equity) >= epsilon ||
                std::fabs(asset->frozen - pre->frozen) >= epsilon ||
                std::fabs(asset->long_margin_usable - pre->long_margin_usable) >= epsilon ||
                std::fabs(asset->short_margin_usable - pre->short_margin_usable) >= epsilon ||
                std::fabs(asset->short_return_usable - pre->short_return_usable) >= epsilon) {
                flag = true;
                assets_[fund_id] = *asset;
            }
        }
        if (flag) {
            LOG_INFO << "[DATA][ASSET] update asset: fund_id: " << fund_id
                 << ", timestamp: " << asset->timestamp
                 << ", balance: " << asset->balance
                 << ", usable: " << asset->usable
                 << ", margin: " << asset->margin
                 << ", equity: " << asset->equity
                 << ", long_margin_usable: " << asset->long_margin_usable
                 << ", short_margin_usable: " << asset->short_margin_usable
                 << ", short_return_usable: " << asset->short_return_usable;
            void* buffer = rep_writer_.OpenFrame(sizeof(MemTradeAsset));
            memcpy(buffer, asset, sizeof(MemTradeAsset));
            rep_writer_.CloseFrame(kMemTypeTradeAsset);
        }
    }
}

void MemBrokerServer::SendQueryTradePositionRep(MemGetTradePositionMessage* rep) {
    int64_t ms = x::SubRawDateTime(x::RawDateTime(), rep->timestamp);
    std::string fund_id = rep->fund_id;
    std::string error = rep->error;
    QueryContext* ctx = nullptr;
    auto itr_cursor = position_contexts_.find(fund_id);
    if (itr_cursor != position_contexts_.end()) {
        ctx = itr_cursor->second;
    }
    if (ctx) {
        int64_t now = x::Timestamp();
        ctx->set_rep_time(now);
        if (error.empty()) {
            ctx->set_last_success_time(now);
        }
    }
    std::string id = rep->id;
    if (x::StartsWith(id, "INIT_OPTION_")) {
        broker_->SetInitPositions(rep, kTradeTypeOption);
        return;
    } else if (x::StartsWith(id, "INIT_STOCK_")) {
        broker_->SetInitPositions(rep, kTradeTypeSpot);
        return;
    }
    wait_size_--;
    if (!error.empty()) {
        LOG_ERROR << "[REP]query position failed in " << ms << "ms: " << error;
        return;
    }
    auto item = (MemTradePosition*)((char*)rep + sizeof(MemGetTradePositionMessage));
    for (int i = 0; i < rep->items_size; i++) {
        MemTradePosition *position = item + i;
        if (position->timestamp == 0) {
            position->timestamp = rep->timestamp;
        }
        bool flag = false;
        string fund_id = position->fund_id;
        string code = position->code;
        auto it = positions_.find(fund_id);
        if (it == positions_.end()) {
            flag = true;
            std::shared_ptr<std::map<std::string, MemTradePosition>> pos = std::make_shared<std::map<std::string, MemTradePosition>>();
            pos->insert(std::make_pair(code, *position));
            positions_.insert(std::make_pair(fund_id, pos));
        } else {
            double epsilon = 0.01;
            std::shared_ptr<std::map<std::string, MemTradePosition>> pos = it->second;
            auto itor = pos->find(code);
            if (itor == pos->end()) {
                flag = true;
                pos->insert(std::make_pair(code, *position));
            } else {
                MemTradePosition* pre = &itor->second;
                double epsilon = 0.01;
                if (position->long_volume != pre->long_volume ||
                    std::fabs(position->long_market_value - pre->long_market_value) >= epsilon ||
                    position->long_can_close != pre->long_can_close ||
                    position->short_volume != pre->short_volume ||
                    std::fabs(position->short_market_value - pre->short_market_value) >= epsilon ||
                    position->short_can_close != pre->short_can_close) {
                    flag = true;
                    itor->second = *position;
                }
            }
        }
        pos_code_.insert(code);
        if (flag) {
            LOG_INFO << "[DATA][POSITION] update position: fund_id: " << fund_id
                     << ", timestamp: " << position->timestamp
                     << ", code: " << position->code
                     << ", long_volume: " << position->long_volume
                     << ", long_market_value: " << position->long_market_value
                     << ", long_can_close: " << position->long_can_close
                     << ", short_volume: " << position->short_volume
                     << ", short_market_value: " << position->short_market_value
                     << ", short_can_open: " << position->short_can_open;
            void* buffer = rep_writer_.OpenFrame(sizeof(MemTradePosition));
            memcpy(buffer, position, sizeof(MemTradePosition));
            rep_writer_.CloseFrame(kMemTypeTradePosition);
        }
    }

    // 删除持仓是0，api不返回对应持仓的问题
    auto it = positions_.find(fund_id);
    if (it != positions_.end()) {
        std::shared_ptr<std::map<std::string, MemTradePosition>> pos = it->second;
        for (auto itor = pos->begin(); itor != pos->end();) {
            if (auto item = pos_code_.find(itor->first); item == pos_code_.end()) {
                LOG_INFO << "fund_id: " << fund_id << ", code: " << itor->second.code << " pos is zero";
                void* buffer = rep_writer_.OpenFrame(sizeof(MemTradePosition));
                MemTradePosition* new_pos = (MemTradePosition*) buffer;
                memset(new_pos, 0,  sizeof(MemTradePosition));
                strcpy(new_pos->code, itor->second.code);
                strcpy(new_pos->fund_id, itor->second.fund_id);
                new_pos->timestamp = itor->second.timestamp;
                new_pos->market = itor->second.market;
                rep_writer_.CloseFrame(kMemTypeTradePosition);
                itor = pos->erase(itor);
            } else {
                ++itor;
            }
        }
    }
    pos_code_.clear();
}

void MemBrokerServer::SendQueryTradeKnockRep(MemGetTradeKnockMessage* rep) {
    auto item = (MemTradeKnock*)((char*)rep + sizeof(MemGetTradeKnockMessage));
    int64_t ms = x::SubRawDateTime(x::RawDateTime(), rep->timestamp);
    std::string error = rep->error;
    std::string fund_id = rep->fund_id;
    std::string next_cursor = rep->next_cursor;
    QueryContext* ctx = nullptr;
    auto itr_cursor = knock_contexts_.find(fund_id);
    if (itr_cursor != knock_contexts_.end()) {
        ctx = itr_cursor->second;
    }
    if (ctx) {
        int64_t now = x::Timestamp();
        ctx->set_rep_time(now);
        if (error.empty()) {
            ctx->set_last_success_time(now);
        }
        if (!next_cursor.empty()) {
            ctx->set_next_cursor(next_cursor);
        }
    }
    wait_size_--;
    if (!error.empty()) {
        LOG_ERROR << "[REP]query knock failed in " << ms << "ms: " << error;
        return;
    }
    for (int i = 0; i < rep->items_size; i++) {
        MemTradeKnock *knock = item + i;
        if (IsNewMemTradeKnock(knock)) {
            void* buffer = rep_writer_.OpenFrame(sizeof(MemTradeKnock));
            memcpy(buffer, knock, sizeof(MemTradeKnock));
            MemTradeKnock* knock = (MemTradeKnock*)buffer;
            rep_writer_.CloseFrame(kMemTypeTradeKnock);
            LOG_INFO << "[DATA][KNOCK] update knock, fund_id: " << knock->fund_id
                     << ", code: " << knock->code
                     << ", timestamp: " << knock->timestamp
                     << ", order_no: " << knock->order_no
                     << ", match_no: " << knock->match_no
                     << ", inner_match_no: " << knock->inner_match_no
                     << ", batch_no: " << knock->batch_no
                     << ", bs_flag: " << knock->bs_flag
                     << ", match_type: " << knock->match_type
                     << ", match_volume: " << knock->match_volume
                     << ", match_price: " << knock->match_price
                     << ", match_amount: " << knock->match_amount;
        }
    }
 }

void  MemBrokerServer::SendTradeOrderRep(MemTradeOrderMessage* rep) {
    if (start_time_ > rep->timestamp) {
        return;
    }
    broker_->SendTradeOrderRep(rep);
    if (auto it = pending_orders_.find(rep->id); it != pending_orders_.end()) {
        pending_orders_.erase(it);
    }
    int64_t ms = x::SubRawDateTime(x::RawDateTime(), rep->timestamp);
    if (strlen(rep->error) > 0) {
        LOG_ERROR << "[REP][WaitRep=" << pending_orders_.size()
                  << "] send order failed in " << ms << "ms, rep = " << ToString(rep);
    } else {
        LOG_ERROR << "[REP][WaitRep=" << pending_orders_.size()
                  << "] send order ok in " << ms << "ms, rep = " << ToString(rep);
    }
    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * rep->items_size;
    void* buffer = rep_writer_.OpenFrame(length);
    memcpy(buffer, rep, length);
    rep_writer_.CloseFrame(kMemTypeTradeOrderRep);
}

void  MemBrokerServer::SendTradeWithdrawRep(MemTradeWithdrawMessage* rep) {
    if (start_time_ > rep->timestamp) {
        return;
    }
    if (auto it = pending_withdraws_.find(rep->id); it != pending_withdraws_.end()) {
        pending_withdraws_.erase(it);
    }
    int64_t ms = x::SubRawDateTime(x::RawDateTime(), rep->timestamp);
    if (strlen(rep->error) > 0) {
        LOG_ERROR << "[REP][WaitRep=" << pending_withdraws_.size()
                  << "] send withdraw failed in " << ms << "ms, rep = " << ToString(rep);
    } else {
        LOG_ERROR << "[REP][WaitRep=" << pending_withdraws_.size()
                  << "] send withdraw ok in " << ms << "ms, rep = " << ToString(rep);
    }
    int length = sizeof(MemTradeWithdrawMessage);
    void* buffer = rep_writer_.OpenFrame(length);
    memcpy(buffer, rep, length);
    rep_writer_.CloseFrame(kMemTypeTradeWithdrawRep);
}

void MemBrokerServer::CreateInnerMatchNo(MemTradeKnock* knock) {
     if (strlen(knock->inner_match_no) != 0) {
         return;
     }
    MemTradeAccount* account = broker_->GetAccount(knock->fund_id);
    if (account) {
        if (account->type == co::kTradeTypeSpot) {
            size_t index = 0;
            size_t i = 0;
            for (i = 0; i < strlen(knock->order_no); ++i) {
                knock->inner_match_no[index++] = knock->order_no[i];
            }
            knock->inner_match_no[index++] = '_';
            for (i = 0; i < strlen(knock->match_no); ++i) {
                knock->inner_match_no[index++] = knock->match_no[i];
            }
            knock->inner_match_no[index++] = '_';
            for (i = 0; i < strlen(knock->code); ++i) {
                knock->inner_match_no[index++] = knock->code[i];
            }
        } else {
            strcpy(knock->inner_match_no, knock->match_no);
        }
    } else {
        strcpy(knock->inner_match_no, knock->match_no);
    }
}

void  MemBrokerServer::SendTradeKnock(MemTradeKnock* knock) {
    if (IsNewMemTradeKnock(knock)) {
        broker_->SendTradeKnock(knock);
        int length = sizeof(MemTradeKnock);
        void* buffer = rep_writer_.OpenFrame(length);
        memcpy(buffer, knock, length);
        rep_writer_.CloseFrame(kMemTypeTradeKnock);
        LOG_INFO << "knock, fund_id: " << knock->fund_id
                 << ", code: " << knock->code
                 << ", timestamp: " << knock->timestamp
                 << ", order_no: " << knock->order_no
                 << ", match_no: " << knock->match_no
                 << ", batch_no: " << knock->batch_no
                 << ", bs_flag: " << knock->bs_flag
                 << ", match_type: " << knock->match_type
                 << ", match_volume: " << knock->match_volume
                 << ", match_price: " << knock->match_price
                 << ", match_amount: " << knock->match_amount;
    }
}

void MemBrokerServer::SendRtnMessage(const std::string& raw, int64_t type) {
    queue_->Push(nullptr, type, raw);
 }

void  MemBrokerServer::RunWatch() {
    int64_t watch_interval_ms = 1000;
    while (true) {
        x::Sleep(watch_interval_ms);
        queue_->Push(nullptr, kMemTypeInnerCyclicSignal, "");
    }
}

void  MemBrokerServer::DoWatch() {
    int64_t timeout_ms = 60000;
    int64_t now = x::RawDateTime();
    int64_t ms = x::SubRawDateTime(now, last_heart_beat_);
    if (ms > 10000) {  // 10秒钟一次心跳
        last_heart_beat_ = now;
        for (auto& it : asset_contexts_) {
            void* buffer = rep_writer_.OpenFrame(sizeof(HeartBeatMessage));
            HeartBeatMessage* msg = (HeartBeatMessage*)buffer;
            strcpy(msg->fund_id, it.first.c_str());
            msg->timestamp = now;
            rep_writer_.CloseFrame(kMemTypeHeartBeat);
        }
    }
    std::string text;
    int64_t timeout_orders = 0;
    int64_t timeout_withdraws = 0;
    for (auto itr = pending_orders_.begin(); itr != pending_orders_.end();) {
        auto ts = itr->second;
        int64_t delay = x::SubRawDateTime(now, ts);
        if (delay >= timeout_ms) {
            string id = itr->first;
            itr = pending_orders_.erase(itr);
            ++timeout_orders;
            auto wait_size = pending_orders_.size();
            x::Error([=]() {
                LOG_ERROR << "[TIMEOUT][WaitRep=" << pending_orders_.size()
                          << "] send order timeout in " << delay << "ms, id: " << id;
            });
        } else {
            ++itr;
        }
    }
    for (auto itr = pending_withdraws_.begin(); itr != pending_withdraws_.end(); ) {
        auto ts = itr->second;
        int64_t delay = x::SubRawDateTime(now, ts);
        if (delay >= timeout_ms) {
            string id = itr->first;
            itr = pending_withdraws_.erase(itr);
            ++timeout_withdraws;
            auto wait_size = pending_withdraws_.size();
            x::Error([=]() {
                LOG_ERROR << "[TIMEOUT][WaitRep=" << pending_withdraws_.size()
                          << "] send withdraw timeout in " << delay << "ms: broker = "
                          << delay << "ms, id = " << id;
            });
        } else {
            ++itr;
        }
    }

    if (text.empty() && (timeout_orders > 0 || timeout_withdraws > 0)) {
        LOG_WARN << "[watchdog] timeout messages: order = " << timeout_orders << ", withdraw = " << timeout_withdraws;
        std::stringstream ss;
        ss << "柜台报单异常：【" << node_name_ << "】出现";
        if (timeout_orders > 0) {
            ss << timeout_orders << "笔报单超时";
        } else {
            ss << timeout_withdraws << "笔撤单超时";
        }
        text = ss.str();
    }

    if (text.empty() && active_task_timestamp_ > 0) {
        int64_t begin_ts = active_task_timestamp_;
        if (begin_ts > 0) {
            int64_t delay_s = x::SubRawDateTime(x::Timestamp(), begin_ts) / 1000;
            if (delay_s > 10) {
                LOG_WARN << "[watchdog] broker has been blocked for " << delay_s << "s";
                if (delay_s < 60) {
                    text = "柜台状态异常：【" + node_name_ + "】无响应超过：" + std::to_string(delay_s) + "秒";
                } else {
                    text = "柜台状态异常：【" + node_name_ + "】无响应超过1分钟";
                }
            }
        }
    }

    if (text.empty() && !asset_contexts_.empty()) {
        int64_t min_time = 0;
        for (auto& itr : asset_contexts_) {
            auto ctx = itr.second;
            if (min_time <= 0 || ctx->last_success_time() < min_time) {
                min_time = ctx->last_success_time();
            }
        }
        int64_t delay_s = (x::Timestamp() - min_time) / 1000;
        if (delay_s > 60) {
            LOG_WARN << "[watchdog] query asset failed for " << delay_s << "s";
            text = "柜台状态异常：【" + node_name_ + "】查询资金失败超过：" + std::to_string(delay_s) + "秒";
        }
    }
    if (text.empty() && !position_contexts_.empty()) {
        int64_t min_time = 0;
        for (auto& itr : position_contexts_) {
            auto ctx = itr.second;
            if (min_time <= 0 || ctx->last_success_time() < min_time) {
                min_time = ctx->last_success_time();
            }
            LOG_INFO << "query pos contexts, fund_id: " << ctx->fund_id() << ", req_time: " << ctx->req_time()
                            << ", rep_time: " << ctx->rep_time() << ", last_success_time: " << ctx->last_success_time();
        }
        int64_t delay_s = (x::Timestamp() - min_time) / 1000;
        if (delay_s > 60) {
            LOG_WARN << "[watchdog] query position failed for " << delay_s << "s";
            text = "柜台状态异常：【" + node_name_ + "】查询持仓失败超过：" + std::to_string(delay_s) + "秒";
        }
    }

    if (text.empty() && !knock_contexts_.empty()) {
        int64_t min_time = 0;
        for (auto& itr : knock_contexts_) {
            auto ctx = itr.second;
            if (min_time <= 0 || ctx->last_success_time() < min_time) {
                min_time = ctx->last_success_time();
            }
        }
        int64_t delay_s = (x::Timestamp() - min_time) / 1000;
        if (delay_s > 60) {
            LOG_WARN << "[watchdog] query knock failed for " << delay_s << "s";
            text = "柜台状态异常：【" + node_name_ + "】查询成交失败超过：" + std::to_string(delay_s) + "秒";
        }
    }

    if (!text.empty()) {
        LOG_ERROR << text;
        int length = sizeof(MemMonitorRiskMessage);
        void* buffer = rep_writer_.OpenFrame(length);
        MemMonitorRiskMessage* msg = (MemMonitorRiskMessage*) buffer;
        msg->timestamp = x::RawDateTime();
        strncpy(msg->error, text.c_str(), text.length() > sizeof(msg->error) ? sizeof(msg->error): text.length());
        rep_writer_.CloseFrame(kMemTypeMonitorRisk);
    }
}
}  // namespace co

