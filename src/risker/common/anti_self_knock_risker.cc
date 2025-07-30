// Copyright 2025 Fancapital Inc.  All rights reserved.
#include "anti_self_knock_risker.h"
#include "yaml-cpp/yaml.h"
#include "order_book.h"

namespace co {
AntiSelfKnockOption::AntiSelfKnockOption(std::shared_ptr<RiskOptions> opt) {
    fund_id_ = opt->fund_id();
    only_etf_ = opt->GetBool("prevent_self_knock_only_etf");
    tag_ = "[" + fund_id_ + "-" + opt->GetStr("name") + "]";
}

void AntiSelfKnockRisker::AddOption(std::shared_ptr<RiskOptions> opt) {
    auto account = new AntiSelfKnockOption(opt);
    LOG_INFO << "[risk][anti_self_knock] add account: " << account->tag()
        << ", only_etf: " << std::boolalpha << account->only_etf();
    options_[opt->fund_id()] = account;
}

std::string AntiSelfKnockRisker::GetAccountInfo(const std::string& fund_id) {
    auto itr = options_.find(fund_id);
    if (itr != options_.end()) {
        return itr->second->tag();
    }
    return fund_id;
}

AntiSelfKnockRisker::AntiSelfKnockRisker() {
}

AntiSelfKnockRisker::~AntiSelfKnockRisker() {
    for (auto& itr: options_) {
        delete itr.second;
    }
    options_.clear();
}

std::string AntiSelfKnockRisker::HandleTradeOrderReq(MemTradeOrderMessage* req) {
    std::string message_id = req->id;
    std::string fund_id = req->fund_id;
    auto opt = GetOption(fund_id);
    if (!opt) {
        return "";
    }
    bool only_etf = opt->only_etf();
    MemTradeOrder* items = req->items;
    for (int i = 0; i < req->items_size; i++) {
        MemTradeOrder* order = items + i;
        std::string code = order->code;
        if (!only_etf || IsETF(code)) {
            auto book = MustGetOrderBook(code);
            std::string error = book->HandleTradeOrderReq(order, req->bs_flag);
            if (!error.empty()) {
                return error;
            }
        }
    }
    return "";
}

void AntiSelfKnockRisker::OnTradeOrderReqPass(MemTradeOrderMessage* req) {
    std::string message_id = req->id;
    std::string fund_id = req->fund_id;
    auto opt = GetOption(fund_id);
    if (!opt) {
        return;
    }
    bool only_etf = opt->only_etf();
    MemTradeOrder* items = req->items;
    for (int i = 0; i < req->items_size; i++) {
        MemTradeOrder* item = items + i;
        std::string code = item->code;
        if (!only_etf || IsETF(code)) {
            auto order = std::make_shared<Order>();
            order->message_id = message_id;
            order->timestamp = req->timestamp;
            order->fund_id = fund_id;
            order->code = code;
            order->bs_flag = req->bs_flag;
            order->volume = item->volume;
            order->price = item->price;
            auto book = MustGetOrderBook(code);
            book->OnTradeOrderReqPass(order);
        }
    }
}

void AntiSelfKnockRisker::HandleTradeOrderRep(MemTradeOrderMessage* rep) {
    std::string fund_id = rep->fund_id;
    std::string batch_no = rep->batch_no;
    auto opt = GetOption(fund_id);
    if (!opt) {
        return;
    }
    std::unique_ptr<std::vector<MemTradeKnock>> knocks;
    bool only_etf = opt->only_etf();
    std::unique_ptr<std::vector<OrderPtr>> orders = nullptr;
    MemTradeOrder* items = rep->items;
    for (int i = 0; i < rep->items_size; i++) {
        MemTradeOrder* order = items + i;
        std::string code = order->code;
        if (!only_etf || IsETF(code)) {
            std::string order_no = order->order_no;
            auto book = MustGetOrderBook(code);
            auto active_order = book->HandleTradeOrderRep(rep, order);
            if (active_order) {
                if (!order_no.empty()) {
                    std::string key = CreateOrderNoKey(fund_id, order_no);
                    // 先收到成交回报, 后收到报单响应
                    if (auto it = knock_first_orders_.find(key); it != knock_first_orders_.end()) {
                        knocks = std::move(it->second);
                        knock_first_orders_.erase(it);
                    }
                    single_orders_[key] = active_order;
                }
                if (!batch_no.empty()) {
                    if (!orders) {
                        orders = std::make_unique<std::vector<OrderPtr>>();
                    }
                    orders->emplace_back(active_order);
                }
            } else {
                LOG_INFO << "order is null, fund_id: " << rep->fund_id
                                << ", code: " << order->code
                                << ", order_no: " << order->order_no
                                << ", error: " << order->error;
            }
        }
    }
    if (orders) {
        std::string batch_key = CreateOrderNoKey(fund_id, batch_no);
        batch_orders_[batch_key] = std::move(orders);
    }
    if (knocks) {
        for (auto& it : *knocks) {
            OnTradeKnock(&it);
        }
    }
}

// 复用withdraw_failed_time字段，只要有撤单请求，过段时间就删掉挂单
std::string AntiSelfKnockRisker::HandleTradeWithdrawReq(MemTradeWithdrawMessage* req) {
    std::string fund_id = req->fund_id;
    std::string batch_no = req->batch_no;
    std::string order_no = req->order_no;
    if (!batch_no.empty()) {
        std::string key = CreateOrderNoKey(fund_id, batch_no);
        auto itr = batch_orders_.find(key);
        if (itr != batch_orders_.end()) {
            auto& orders = itr->second;
            for (auto& order: *orders) {
                order->withdraw_failed_time = x::UnixMilli();
            }
        }
    } else if (!order_no.empty()) {
        std::string key = CreateOrderNoKey(fund_id, order_no);
        auto itr = single_orders_.find(key);
        if (itr != single_orders_.end()) {
            auto& order = itr->second;
            order->withdraw_failed_time = x::UnixMilli();
        }
    }
    return "";
}

void AntiSelfKnockRisker::HandleTradeWithdrawRep(MemTradeWithdrawMessage* rep) {
    std::string fund_id = rep->fund_id;
    std::string batch_no = rep->batch_no;
    std::string order_no = rep->order_no;
    std::string error = rep->error;
    if (!batch_no.empty()) {
        std::string key = CreateOrderNoKey(fund_id, batch_no);
        auto itr = batch_orders_.find(key);
        if (itr != batch_orders_.end()) {
            auto& orders = itr->second;
            if (!error.empty()) {  // 撤单失败
                // @TODO 如果确认是柜台返回的撤单失败，说明委托已经被撤单了，或者已经成交完了；
                // 如果仅仅是自由系统内部返回的撤单失败，不应该处理。
                for (auto& order: *orders) {
                    order->withdraw_failed_time = x::UnixMilli();
                }
            } else {  // 撤单成功
                for (auto& order: *orders) {
                    order->withdraw_succeed = true;
                }
                batch_orders_.erase(itr);
            }
        }
    } else if (!order_no.empty()) {
        std::string key = CreateOrderNoKey(fund_id, order_no);
        auto itr = single_orders_.find(key);
        if (itr != single_orders_.end()) {
            auto& order = itr->second;
            if (!error.empty()) {  // 撤单失败
                order->withdraw_failed_time = x::UnixMilli();
            } else {  // 撤单成功
                order->withdraw_succeed = true;
            }
        }
    }
}

void AntiSelfKnockRisker::OnTradeKnock(MemTradeKnock* knock) {
    // 交易网关能保证同一个成交回报不会多次调用该函数，所以这里不用考虑重复成交回报的问题；
    // 如果要考虑重复成交的话，可以在Order中增加一个map，用于保存成交回报；
    std::string fund_id = knock->fund_id;
    std::string order_no = knock->order_no;
    std::string key = CreateOrderNoKey(fund_id, order_no);
    auto itr = single_orders_.find(key);
    if (itr != single_orders_.end()) {
        auto& order = itr->second;
        if (knock->match_type == kMatchTypeOK) {
            order->match_volume += knock->match_volume;
        } else if (knock->match_type == kMatchTypeWithdrawOK ||
            knock->match_type == kMatchTypeFailed) {
            order->withdraw_succeed = true;
        }
        if (order->IsFinished()) {
            OnOrderFinish(order);
        }
    } else {
        if (knock->match_type == kMatchTypeWithdrawOK) {
            return;
        }
        // 先收到成交回报, 后收到报单响应
        auto opt = GetOption(fund_id);
        if (!opt) {
            return;
        }
        std::string code = knock->code;
        bool only_etf = opt->only_etf();
        if (!only_etf || IsETF(code)) {
            auto it = knock_first_orders_.find(key);
            if (it == knock_first_orders_.end()) {
                std::unique_ptr<std::vector<MemTradeKnock>> knocks = std::make_unique<std::vector<MemTradeKnock>>();
                knocks->push_back(*knock);
                knock_first_orders_.insert(std::make_pair(key, std::move(knocks)));
            } else {
                it->second->push_back(*knock);
            }
            std::string match_no = knock->match_no;
            LOG_INFO << "knock first, order_no second, fund_id: " << fund_id
                     << ", code: " << code
                     << ", order_no: " << order_no
                     << ", match_no: " << match_no
                     << ", match_type: " << knock->match_type
                     << ", match_volume: " << knock->match_volume;
        }
    }
}

AntiSelfKnockOption* AntiSelfKnockRisker::GetOption(const std::string& fund_id) {
    AntiSelfKnockOption* ret = nullptr;
    auto itr = options_.find(fund_id);
    if (itr != options_.end()) {
        ret = itr->second;
    }
    return ret;
}

OrderBook* AntiSelfKnockRisker::MustGetOrderBook(const std::string& code) {
    OrderBook* book = nullptr;
    auto itr = order_books_.find(code);
    if (itr != order_books_.end()) {
        book = itr->second;
    } else {
        book = new OrderBook(this);
        order_books_[code] = book;
    }
    return book;
}

OrderBook* AntiSelfKnockRisker::TryGetOrderBook(const std::string& code) {
    OrderBook* book = nullptr;
    auto itr = order_books_.find(code);
    if (itr != order_books_.end()) {
        book = itr->second;
    }
    return book;
}

bool AntiSelfKnockRisker::IsETF(const std::string& code) {
    bool ok = false;
    if (code.size() == 9) {
        std::string code_prefix1 = code.substr(0, 3);
        std::string code_prefix2 = code.substr(0, 2);
        std::string code_suffix = code.substr(6);
        if ((x::EndsWith(code, co::kSuffixSZ) && x::StartsWith(code, "159")) ||
            (x::EndsWith(code, co::kSuffixSH) && x::StartsWith(code, "5"))) {
            ok = true;
        }
    }
    return ok;
}

std::string AntiSelfKnockRisker::CreateOrderNoKey(const std::string_view& fund_id, const std::string_view& key) {
    std::string result;
    result.reserve(fund_id.size() + 1 + key.size());
    result += fund_id;
    result += "#";
    result += key;
    return result;
}

void AntiSelfKnockRisker::OnTick(MemQTickBody* tick) {
    std::string code = tick->code;
    auto book = TryGetOrderBook(code);
    if (book) {
        book->OnTick(tick);
    }
}

void AntiSelfKnockRisker::OnOrderFinish(OrderPtr order) {
    if (!order->batch_no.empty()) {
        std::string key = CreateOrderNoKey(order->fund_id, order->batch_no);
        auto itr = batch_orders_.find(key);
        if (itr != batch_orders_.end()) {
            auto& orders = itr->second;
            bool all_finished = true;
            for (auto& active_order: *orders) {
                if (!active_order->IsFinished()) {
                    all_finished = false;
                    break;
                }
            }
            if (all_finished) {
                batch_orders_.erase(itr);
            }
        }
    }
    if (!order->order_no.empty()) {
        std::string key = CreateOrderNoKey(order->fund_id, order->order_no);
        single_orders_.erase(key);
    }
}
}  // namespace co
