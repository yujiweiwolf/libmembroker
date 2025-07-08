// Copyright 2021 Fancapital Inc.  All rights reserved.
#include "order_book.h"
#include "anti_self_knock_risker.h"

namespace co {

Order::Order()
: create_time(x::UnixMilli()),
timestamp(0),
bs_flag(0),
price(0),
volume(0),
match_volume(0),
withdraw_failed_time(0),
finish_flag(false) {
}

bool Order::IsFinished() {
    if (finish_flag) {
        return finish_flag;
    }
    if (withdraw_succeed || match_volume >= volume) {
        finish_flag = true;
        return finish_flag;
    }
    int64_t now = x::UnixMilli();
    // 等待委托响应超时
    if (order_no.empty() && (now - create_time) > kOrderTimeoutMS) {
        LOG_INFO << "委托响应超时, code: " << code
           << ", message_id: " << message_id
           << ", bs_flag: " << bs_flag
           << ", price: " << price
           << ", volume: " << volume;
        finish_flag = true;
        return finish_flag;
    }
    // 撤单失败超过设定阈值
    if (order_no.length() > 0 && withdraw_failed_time > 0 && (now - withdraw_failed_time) > kOrderTimeoutMS) {
        LOG_INFO << "撤单失败超过设定阈值, code: " << code
                 << ", message_id: " << message_id
                 << ", order_no: " << order_no
                 << ", bs_flag: " << bs_flag
                 << ", price: " << price
                 << ", volume: " << volume;
        finish_flag = true;
        return finish_flag;
    }
    return finish_flag;
}

OrderBook::OrderBook(AntiSelfKnockRisker* risker): risker_(risker) {

}

std::string OrderBook::HandleTradeOrderReq(MemTradeOrder* order, int64_t bs_flag) {
    // 集合竞价期间，不处理
    int64_t timestamp = x::RawDateTime();
    int64_t stamp = timestamp % 1000000000LL;
    if (stamp < 93000000 || (stamp >= 145700000 && stamp < 150100000)) {
        std::string code = order->code;
        LOG_INFO << "auction time, disregard order, code: " << code
                 << ", bs_flag: " << bs_flag
                 << ", price: " << order->price
                 << ", volume: " << order->volume
                 << ", local stamp: " << stamp;
        return "";
    }
    TryHandleTick();
    latest_order_timestamp_ = timestamp;
    int64_t order_price = EncodePrice(order->price);

    if (bs_flag == kBsFlagBuy) {
        for (auto itr = asks_.begin(); itr != asks_.end();) {
            int64_t ask_price = itr->first;
            if (order_price < ask_price) {
                break;
            }
            auto active_order = itr->second;
            bool remove = active_order->IsFinished();
            if (remove) {
                itr = asks_.erase(itr);
                risker_->OnOrderFinish(active_order);
            } else {
                std::stringstream ss;
                ss << "[FAN-RISK-ERROR][防对敲]风控检查失败，存在卖出挂单："
                << risker_->GetAccountInfo(active_order->fund_id)
                << "[" << x::RawTimeText(active_order->timestamp%1000000000LL) << "]"
                << "[" << active_order->code << "]"
                << " price: " << active_order->price
                << ", volume: " << active_order->volume
                << ", order_no: " << active_order->order_no;
                return ss.str();
            }
        }
    } else if (bs_flag == kBsFlagSell) {
        for (auto itr = bids_.begin(); itr != bids_.end();) {
            int64_t bid_price = itr->first;
            if (order_price > bid_price) {
                break;
            }
            auto active_order = itr->second;
            bool remove = active_order->IsFinished();
            if (remove) {
                itr = bids_.erase(itr);
                risker_->OnOrderFinish(active_order);
            } else {
                std::stringstream ss;
                ss << "[FAN-RISK-ERROR][防对敲]风控检查失败，存在买入挂单："
                << risker_->GetAccountInfo(active_order->fund_id)
                << "[" << x::RawTimeText(active_order->timestamp%1000000000LL) << "]"
                << "[" << active_order->code << "]"
                << " price: " << active_order->price
                << ", volume: " << active_order->volume
                << ", order_no: " << active_order->order_no;
                return ss.str();
            }
        }
    }
    return "";
}

void OrderBook::OnTradeOrderReqPass(OrderPtr order) {
    int64_t order_price = EncodePrice(order->price);
    if (order->bs_flag == kBsFlagBuy) {
        bids_.insert(std::make_pair(order_price, order));
    } else if (order->bs_flag == kBsFlagSell) {
        asks_.insert(std::make_pair(order_price, order));
    }
}

OrderPtr OrderBook::HandleTradeOrderRep(MemTradeOrderMessage* rep, MemTradeOrder* order) {
    OrderPtr ret = nullptr;
    int64_t order_price = EncodePrice(order->price);
    std::string order_no = order->order_no;
    bool inner_flag = false;
    if (rep->bs_flag == kBsFlagBuy) {
        auto range = bids_.equal_range(order_price);
        for (auto itr = range.first; itr != range.second; ++itr) {
            auto& active_order = itr->second;
            if (active_order->message_id.compare(rep->id) == 0) {
                inner_flag = true;
                if (order_no.empty()) {
                    bids_.erase(itr);
                } else {
                    active_order->batch_no = rep->batch_no;
                    active_order->order_no = order_no;
                    ret = active_order;
                }
                break;
            }
        }
        // 其它帐号的单子，只有响应
        if (!inner_flag && strlen(order->order_no) > 0) {
            ret = std::make_shared<Order>();
            ret->message_id = rep->id;
            ret->timestamp = rep->timestamp;
            ret->fund_id = rep->fund_id;
            ret->code = order->code;
            ret->bs_flag = rep->bs_flag;
            ret->volume = order->volume;
            ret->price = order->price;
            ret->batch_no = rep->batch_no;
            ret->order_no = order_no;
            OnTradeOrderReqPass(ret);
        }
    } else if (rep->bs_flag == kBsFlagSell) {
        auto range = asks_.equal_range(order_price);
        for (auto itr = range.first; itr != range.second; ++itr) {
            auto& active_order = itr->second;
            if (active_order->message_id.compare(rep->id) == 0) {
                inner_flag = true;
                if (order_no.empty()) {
                    asks_.erase(itr);
                } else {
                    active_order->batch_no = rep->batch_no;
                    active_order->order_no = order_no;
                    ret = active_order;
                }
                break;
            }
        }
        // 其它broker的单子，只有响应
        if (!inner_flag && strlen(order->order_no) > 0) {
            ret = std::make_shared<Order>();
            ret->message_id = rep->id;
            ret->timestamp = rep->timestamp;
            ret->fund_id = rep->fund_id;
            ret->code = order->code;
            ret->bs_flag = rep->bs_flag;
            ret->volume = order->volume;
            ret->price = order->price;
            ret->batch_no = rep->batch_no;
            ret->order_no = order_no;
            OnTradeOrderReqPass(ret);
        }
    }
    return ret;
}

void OrderBook::OnOrderFinish(OrderPtr order) {
    risker_->OnOrderFinish(order);
}

void OrderBook::OnTick(MemQTickBody* q) {
    if (latest_order_timestamp_ > 0 && x::SubRawDateTime(q->timestamp, latest_order_timestamp_) < 10000) {
        // 由于风控模块处理交易数据和行情数据并不是严格按照时间先后顺序进行的，所以可能会出现行情数据延迟的情况，
        // 这里只处理时间大于最新委托时间的行情数据，而且给一个5秒钟的余量；
        return;
    }
    // @TODO 注意：如果报单很密集，则上面的条件一致处于成立状态，行情永远也无法用来删除订单；
    double bp1 = q->bp[0];
    double ap1 = q->ap[0];
    double new_price = q->new_price;
    if (bp1 > 0) {
        if (max_bp1_ <= 0 || bp1 > max_bp1_) {
            has_tick_ = true;
            max_bp1_ = bp1;
        }
    }
    if (ap1 > 0) {
        if (min_ap1_ <= 0 || ap1 < min_ap1_) {
            has_tick_ = true;
            min_ap1_ = ap1;
        }
    }
    if (new_price > 0) {
        if (min_new_price_ <= 0 || new_price < min_new_price_) {
            has_tick_ = true;
            min_new_price_ = new_price;
        }
        if (max_new_price_ <= 0 || new_price > max_new_price_) {
            has_tick_ = true;
            max_new_price_ = new_price;
        }
    }
}

void OrderBook::TryHandleTick() {
    if (!has_tick_) {
        return;
    }
    has_tick_ = false;
    int64_t max_bp1 = max_bp1_ > 0 ? EncodePrice(max_bp1_) : 0;
    int64_t min_ap1 = min_ap1_ > 0 ? EncodePrice(min_ap1_) : 0;
    int64_t max_new_price = max_new_price_ > 0 ? EncodePrice(max_new_price_) : 0;
    int64_t min_new_price = min_new_price_ > 0 ? EncodePrice(min_new_price_) : 0;
    int64_t max_bid = max_new_price > 0 ? max_new_price : 0;
    if (max_bp1 > 0 && max_bp1 > max_bid) {
        max_bid = max_bp1;
    }
    int64_t min_ask = min_new_price > 0 ? min_new_price : 0;
    if (min_ap1 > 0 && min_ap1 < min_ask) {
        min_ask = min_ap1;
    }
    if (max_bid > 0) {
        for (auto itr = asks_.begin(); itr != asks_.end();) {
            int64_t ask_price = itr->first;
            if (max_bid < ask_price) {
                break;
            }
            if ((max_bp1 > 0 && max_bp1 >= ask_price) ||
                (max_new_price > 0 && max_new_price > ask_price)) {
                LOG_INFO << "delete ask order by tick, code: " << itr->second->code
                << ", order_no: " << itr->second->order_no
                << ", ask_price: " << ask_price
                << ", max_bp1: " << max_bp1
                << ", max_new_price: " << max_new_price
                << ", min_ap1: " << min_ap1
                << ", min_new_price: " << min_new_price;
                itr = asks_.erase(itr);
            } else {
                ++itr;
            }
        }
    }
    if (min_ask > 0) {
        for (auto itr = bids_.begin(); itr != bids_.end();) {
            int64_t bid_price = itr->first;
            if (min_ask > bid_price) {
                break;
            }
            if ((min_ap1 > 0 && min_ap1 <= bid_price) ||
            (min_new_price > 0 && min_new_price < bid_price)) {
                LOG_INFO << "delete bid order by tick, code: " << itr->second->code
                << ", order_no: " << itr->second->order_no
                << ", bid_price: " << bid_price
                << ", max_bp1: " << max_bp1
                << ", max_new_price: " << max_new_price
                << ", min_ap1: " << min_ap1
                << ", min_new_price: " << min_new_price;
                itr = bids_.erase(itr);
            } else {
                ++itr;
            }
        }
    }
    ClearTick();
}

void OrderBook::ClearTick() {
    has_tick_ = false;
    max_bp1_  = 0;
    min_ap1_  = 0;
    min_new_price_  = 0;
    max_new_price_  = 0;
}
}  // namespace co
