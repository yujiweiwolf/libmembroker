// Copyright 2025 Fancapital Inc.  All rights reserved.
#pragma once
#include "x/x.h"
#include "coral/coral.h"

namespace co {
const int64_t kOrderTimeoutMS = 3000;
const double kRiskOrderPriceEpsilon = 0.000001;

inline bool PriceEquals(double a, double b) {
    return a - b > -kRiskOrderPriceEpsilon && a - b < kRiskOrderPriceEpsilon;  // 判断价格是否相等
}

inline bool PriceGreatTo(double a, double b) {
    return a - b > kRiskOrderPriceEpsilon;
}

inline bool PriceGreatEquals(double a, double b) {
    return a - b >= -kRiskOrderPriceEpsilon;
}

inline bool PriceLessTo(double a, double b) {
    return a - b < -kRiskOrderPriceEpsilon;
}

inline bool PriceLessEquals(double a, double b) {
    return a - b <= kRiskOrderPriceEpsilon;
}

inline int64_t EncodePrice(double price) {
    return price >= 0 ? static_cast<int64_t> (price * 10000 + 0.5) : static_cast<int64_t> (price * 10000 - 0.5);
}

inline double DecodePrice(int64_t price) {
    return static_cast<double> (price) / 10000;
}

class Order {
 public:
    Order();
    bool IsFinished();

 public:
    int64_t create_time = 0;
    std::string message_id;
    int64_t timestamp = 0;
    std::string fund_id;
    std::string code;
    std::string order_no;
    std::string batch_no;
    int64_t bs_flag = 0;
    double price = 0;
    int64_t volume = 0;
    int64_t match_volume = 0;
    int64_t withdraw_failed_time = 0;  // 撤单失败的时间，如果撤单失败, 过段时间还没收到撤单成交回报, 自成交检查时认为已经撤单//
    bool withdraw_succeed = false;  // 撤单成功标志, 没收到撤单成交回报, 自成交检查时认为已经撤单//
    bool finish_flag = false;
};

typedef std::shared_ptr<Order> OrderPtr;
class AntiSelfKnockRisker;

class OrderBook {
 public:
    OrderBook(AntiSelfKnockRisker* risker);

    std::string HandleTradeOrderReq(MemTradeOrder* order, int64_t bs_flag);
    void OnTradeOrderReqPass(OrderPtr order);
    OrderPtr HandleTradeOrderRep(MemTradeOrderMessage* rep, MemTradeOrder* order);
    void OnTick(MemQTickBody* tick);

 private:
    void OnOrderFinish(OrderPtr order);
    void TryHandleTick();
    void ClearTick();

 private:
    AntiSelfKnockRisker* risker_ = nullptr;
    int64_t latest_order_timestamp_ = 0;  // 最新委托时间
    double has_tick_ = false;
    double max_bp1_ = 0;
    double min_ap1_ = 0;
    double min_new_price_ = 0;
    double max_new_price_ = 0;

    std::multimap<int64_t, OrderPtr, std::less<int64_t> > asks_;  // price -> orders
    std::multimap<int64_t, OrderPtr, std::greater<int64_t> > bids_;  // price -> orders
    std::unordered_map<std::string, OrderPtr> orders_;  // <fund_id>#<order_no> -> order
};
}  // namespace co
