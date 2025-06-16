// Copyright 2021 Fancapital Inc.  All rights reserved.
#include "order_book.h"
#include "anti_self_knock_risker.h"

namespace co {

    Order::Order()
    : create_time(x::Timestamp()),
    timestamp(0),
    bs_flag(0),
    price(0),
    volume(0),
    match_volume(0),
    withdraw_failed_time(0) {
    }

    bool Order::IsFinished(const int64_t& now) const {
        return withdraw_succeed || match_volume >= volume ||
        (now > 0 && withdraw_failed_time > 0 && now - withdraw_failed_time > kOrderTimeoutMS);
    }

    OrderBook::OrderBook(AntiSelfKnockRisker* risker): risker_(risker) {

    }

    std::string OrderBook::HandleTradeOrderReq(MemTradeOrder* order, int64_t bs_flag) {
        // 集合竞价期间，不处理
        int64_t stamp = x::RawTime();
        if (stamp < 93000000 || stamp >= 145700000) {
            std::string code = order->code;
            LOG_INFO << "auction time, disregard order, code: " << code
                     << ", bs_flag: " << bs_flag
                     << ", price: " << order->price
                     << ", volume: " << order->volume
                     << ", local stamp: " << stamp;
            return "";
        }
        TryHandleTick();
        latest_order_timestamp_ = x::RawDateTime();
        int64_t order_price = EncodePrice(order->price);
        int64_t now = x::Timestamp();
        if (bs_flag == kBsFlagBuy) {
            for (auto itr = asks_.begin(); itr != asks_.end();) {
                int64_t ask_price = itr->first;
                if (order_price < ask_price) {
                    break;
                }
                auto active_order = itr->second;
                bool remove = false;
                if (active_order->order_no.empty() && now - active_order->create_time > kOrderTimeoutMS) {
                    remove = true;  // 如果等待委托响应超时，则删除挂单
                }
                if (!remove) {
                    if (active_order->IsFinished() ||
                    (active_order->withdraw_failed_time > 0 &&
                        now - active_order->withdraw_failed_time > kOrderTimeoutMS)) {
                        remove = true;  // 如果已撤单成功，或者撤单失败超过设定阈值，则删除挂单
                    }
                }
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
                bool remove = false;
                if (active_order->order_no.empty() && now - active_order->create_time > kOrderTimeoutMS) {
                    remove = true;  // 如果等待委托响应超时，则删除挂单
                }
                if (!remove) {
                    if (active_order->IsFinished() ||
                    (active_order->withdraw_failed_time > 0 &&
                    now - active_order->withdraw_failed_time > kOrderTimeoutMS)) {
                        remove = true;  // 如果已撤单成功，或者撤单失败超过设定阈值，则删除挂单
                    }
                }
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
    }

    void OrderBook::OnTradeOrderReqPass(OrderPtr order) {
        int64_t order_price = EncodePrice(order->price);
        if (order->bs_flag == kBsFlagBuy) {
            bids_.insert(std::make_pair(order_price, order));
        } else if (order->bs_flag == kBsFlagSell) {
            asks_.insert(std::make_pair(order_price, order));
        }
    }

    OrderPtr OrderBook::HandleTradeOrderRep(const std::string& message_id,
                                            const std::string& fund_id,
                                            const std::string& batch_no,
                                            int64_t bs_flag,
                                            MemTradeOrder* order) {
        OrderPtr ret = nullptr;
        int64_t order_price = EncodePrice(order->price);
        std::string order_no = order->order_no;
        if (bs_flag == kBsFlagBuy) {
            auto range = bids_.equal_range(order_price);
            for (auto itr = range.first; itr != range.second; ++itr) {
                auto& active_order = itr->second;
                if (active_order->message_id == message_id) {
                    if (order_no.empty()) {
                        bids_.erase(itr);
                    } else {
                        active_order->batch_no = batch_no;
                        active_order->order_no = order_no;
                        ret = active_order;
                    }
                    break;
                }
            }
        } else if (bs_flag == kBsFlagSell) {
            auto range = asks_.equal_range(order_price);
            for (auto itr = range.first; itr != range.second; ++itr) {
                auto& active_order = itr->second;
                if (active_order->message_id == message_id) {
                    if (order_no.empty()) {
                        asks_.erase(itr);
                    } else {
                        active_order->batch_no = batch_no;
                        active_order->order_no = order_no;
                        ret = active_order;
                    }
                    break;
                }
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
