#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <regex>
#include <boost/algorithm/hex.hpp>
#include "mem_server.h"

namespace co {
     MemBrokerServer:: MemBrokerServer() {
         start_time_ = x::RawDateTime();
    }

    void MemBrokerServer::Init(MemBrokerOptionsPtr option, MemBrokerPtr broker) {
        opt_ = option;
        node_name_ = option->node_name();
        x::Sleep(1000);
        broker_ = broker;
        processor_ = std::make_shared<MemProcessor>(this);
        processor_->Init(*opt_);
    }

    void  MemBrokerServer::Start() {
        threads_.emplace_back(std::make_shared<std::thread>(std::bind(& MemBrokerServer::Run, this)));
    }

    void  MemBrokerServer::Join() {
        for (auto& thread : threads_) {
            thread->join();
        }
    }

    void  MemBrokerServer::Run() {
        LOG_INFO << "start broker server ...";
        if (!opt_) {
            throw std::runtime_error("options is required, please initialize broker server before starting");
        }
        if (!broker_) {
            throw std::runtime_error("broker is required, please initialize broker server before starting");
        }

        inner_writer_.Open("../data", kInnerBrokerFile, kInnerBrokerMemSize << 20, true);
        rep_writer_.Open(opt_->mem_dir(), opt_->mem_rep_file(), kRepMemSize << 20, true);

        broker_->Init(*opt_, this, &inner_writer_, &rep_writer_);

        enable_upload_asset_ = opt_->enable_upload() && opt_->query_asset_interval_ms() > 0;
        enable_upload_position_ = opt_->enable_upload() && opt_->query_position_interval_ms() > 0;
        enable_upload_knock_ = opt_->enable_upload();

        threads_.emplace_back(std::make_shared<std::thread>(std::bind(& MemBrokerServer::RunQuery, this)));
        threads_.emplace_back(std::make_shared<std::thread>(std::bind(& MemBrokerServer::RunWatch, this)));
        processor_->Run();  // 放在最后
    }

    bool MemBrokerServer::ExitAccout(const string& fund_id) {
         return broker_->ExitAccout(fund_id);
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
                void* buffer = inner_writer_.OpenFrame(sizeof(MemGetTradePositionMessage));;
                MemGetTradePositionMessage* req = (MemGetTradePositionMessage*)buffer;
                string id = "INIT_STOCK_" + x::UUID();
                req->timestamp = x::RawDateTime();
                strncpy(req->id, id.c_str(), id.length());
                strncpy(req->fund_id, fund_id.c_str(), fund_id.length());
                inner_writer_.CloseFrame(kMemTypeQueryTradePositionReq);
            }
        }

        if (!option_fund_ids.empty()) {
            for (auto& it : option_fund_ids) {
                std::string& fund_id = it;
                LOG_INFO << "query option init position: fund_id = " << fund_id << " ...";
                void* buffer = inner_writer_.OpenFrame(sizeof(MemGetTradePositionMessage));;
                MemGetTradePositionMessage* req = (MemGetTradePositionMessage*)buffer;
                string id = "INIT_OPTION_" + x::UUID();
                req->timestamp = x::RawDateTime();
                strncpy(req->id, id.c_str(), id.length());
                strncpy(req->fund_id, fund_id.c_str(), fund_id.length());
                inner_writer_.CloseFrame(kMemTypeQueryTradePositionReq);
            }
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
                    void* buffer = inner_writer_.OpenFrame(sizeof(MemGetTradeAssetMessage));;
                    MemGetTradeAssetMessage* req = (MemGetTradeAssetMessage*)buffer;
                    string id = x::UUID();
                    string fund_id = ctx->fund_id();
                    req->timestamp = x::RawDateTime();
                    strncpy(req->id, id.c_str(), id.length());
                    strncpy(req->fund_id, fund_id.c_str(), fund_id.length());
                    inner_writer_.CloseFrame(kMemTypeQueryTradeAssetReq);
                }
            }

            for (auto& itr : position_contexts_) {
                auto ctx = itr.second;
                int64_t max_time = ctx->rep_time() >= ctx->req_time() ?
                        ctx->rep_time() : ctx->req_time();
                bool active = ctx->rep_time() < ctx->req_time() ? true : false;
                if ((!active && now - max_time >= query_position_ms) || now - max_time >= position_timeout_ms) {
                    ctx->set_req_time(now);
                    void* buffer = inner_writer_.OpenFrame(sizeof(MemGetTradePositionMessage));;
                    MemGetTradePositionMessage* req = (MemGetTradePositionMessage*)buffer;
                    string id = x::UUID();
                    string fund_id = ctx->fund_id();
                    req->timestamp = x::RawDateTime();
                    strncpy(req->id, id.c_str(), id.length());
//                    int64_t delay_s = (now - start_time) / 1000;
//                    if (delay_s > 100 && delay_s < 200) {
//                        strncpy(req->fund_id, fund_id.c_str(), fund_id.length());
//                        LOG_INFO << "delay_s: " << delay_s;
//                    }
                    strncpy(req->fund_id, fund_id.c_str(), fund_id.length());
                    inner_writer_.CloseFrame(kMemTypeQueryTradePositionReq);
                    LOG_INFO << "query pos contexts, fund_id: " << ctx->fund_id() << ", req_time: " << ctx->req_time();
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
//                LOG_INFO << "req_time: " << ctx->req_time() << ", rep_time: " << ctx->rep_time()
//                << ", now: " << now << ", diff: " << now - max_time << ", knock_timeout_ms: " << knock_timeout_ms;
                bool active = ctx->rep_time() < ctx->req_time() ? true : false;
                if ((!active && now - max_time >= knock_timeout_ms) || now - max_time >= knock_timeout_ms || must_query) {
                    ctx->set_req_time(now);
                    ctx->set_cursor(next_cursor);
                    void* buffer = inner_writer_.OpenFrame(sizeof(MemGetTradeKnockMessage));;
                    MemGetTradeKnockMessage* req = (MemGetTradeKnockMessage*)buffer;
                    string id = x::UUID();
                    string fund_id = ctx->fund_id();
                    req->timestamp = x::RawDateTime();
                    strncpy(req->id, id.c_str(), id.length());
                    strncpy(req->fund_id, fund_id.c_str(), fund_id.length());
                    strncpy(req->cursor, next_cursor.c_str(), next_cursor.length());
                    inner_writer_.CloseFrame(kMemTypeQueryTradeKnockReq);
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

    void  MemBrokerServer::HandInnerCyclicSignal() {
        int64_t now = x::RawDateTime();
        int64_t ms = x::SubRawDateTime(now, last_heart_beat_);
        if (ms > 10000) {  // 10秒钟一次心跳
            last_heart_beat_ = now;
            broker_->SendHeartBeat();
        }
        DoWatch();
    }

    bool MemBrokerServer::MemBrokerServer::IsNewMemTradeKnock(MemTradeKnock* knock) {
        bool flag = false;
        string fund_id = knock->fund_id;
        string key = string(fund_id) + "_" + string(knock->inner_match_no);
        auto it = knocks_.find(fund_id);
        if (it == knocks_.end()) {
            knocks_.insert(key);
            flag = true;
        }
        return flag;
     }

    // 处理api的响应
    void MemBrokerServer::SendQueryTradeAssetRep(MemGetTradeAssetMessage* rep) {
         // 之前的数据
         if (start_time_ > rep->timestamp) {
            return;
         }
        int64_t ms = x::SubRawDateTime(x::RawDateTime(), rep->timestamp);
        std::string error;
        std::string fund_id = rep->fund_id;
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
            x::Error([=]() {
                LOG_ERROR << "[REP]query asset failed in " << ms << "ms: " << error;
            });
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
        // 之前的数据
        if (start_time_ > rep->timestamp) {
            return;
        }
        int64_t ms = x::SubRawDateTime(x::RawDateTime(), rep->timestamp);
        std::string error;
        std::string fund_id = rep->fund_id;
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
            x::Error([=]() {
                LOG_ERROR << "[REP]query position failed in " << ms << "ms: " << error;
            });
            return;
        }
        auto item = (MemTradePosition*)((char*)rep + sizeof(MemGetTradePositionMessage));
        for (int i = 0; i < rep->items_size; i++) {
            MemTradePosition *position = item + i;
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
                         << ", timestamp: " << rep->timestamp
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
        // 之前的数据
        if (start_time_ > rep->timestamp) {
            if (rep->items_size) {
                for (int i = 0; i < rep->items_size; i++) {
                    MemTradeKnock *knock = item + i;
                    IsNewMemTradeKnock(knock);
                }
            }
            return;
        }
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
            x::Error([=]() {
                LOG_ERROR << "[REP]query knock failed in " << ms << "ms: " << error;
            });
            return;
        }
        for (int i = 0; i < rep->items_size; i++) {
            MemTradeKnock *knock = item + i;
            if (IsNewMemTradeKnock(knock)) {
                void* buffer = rep_writer_.OpenFrame(sizeof(MemTradeKnock));
                memcpy(buffer, knock, sizeof(MemTradeKnock));
                MemTradeKnock* knock = (MemTradeKnock*)buffer;
//                stringstream ss;
//                ss << knock->order_no << "_" << knock->match_no << "_" << knock->code;
//                string inner_match_no = ss.str();
//                strcpy(knock->inner_match_no, inner_match_no.c_str());
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
                         << ", match_amount: " << knock->match_amount
                         ;
            }
        }
     }

    void  MemBrokerServer::SendTradeOrderRep(MemTradeOrderMessage* rep) {
        if (start_time_ > rep->timestamp) {
            return;
        }
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
    }

    void  MemBrokerServer::SendTradeKnock(MemTradeKnock* knock) {
        if (IsNewMemTradeKnock(knock)) {
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
                     << ", match_amount: " << knock->match_amount
                     ;
        }
    }

    void  MemBrokerServer::RunWatch() {
        int64_t watch_interval_ms = 5000;
        while (true) {
            x::Sleep(watch_interval_ms);
            int length = sizeof(HeartBeatMessage);
            void* buffer = inner_writer_.OpenFrame(length);
            HeartBeatMessage* msg = (HeartBeatMessage*)buffer;
            msg->timestamp = x::RawDateTime();
            inner_writer_.CloseFrame(kMemTypeInnerCyclicSignal);
        }
    }

    void  MemBrokerServer::DoWatch() {
        int64_t timeout_ms = 60000;
        int64_t now = x::RawDateTime();
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

        if (text.empty() && active_task_timestamp_ > 0) { // 检查调用broker函数是否卡死
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
            broker_->SendRiskMessage(text);
        }
    }
}  // namespace co

