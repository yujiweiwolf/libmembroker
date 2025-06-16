// Copyright 2021 Fancapital Inc.  All rights reserved.
#pragma once
#include <set>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include "../base_risker.h"
#include "order_book.h"
#include "define.h"

namespace co {
class OrderBook;

class AntiSelfKnockOption {
public:
    AntiSelfKnockOption(std::shared_ptr<RiskOptions> opt);

    inline const std::string& fund_id() const {
        return fund_id_;
    }

    inline const std::string& tag() const {
        return tag_;
    }

    inline bool only_etf() const {
        return only_etf_;
    }

private:
    std::string fund_id_;
    std::string tag_;
    bool only_etf_ = false; // 是否只针对ETF进行防对敲
};

struct QTick {
    int64_t timestamp;
    double bp1;
    double ap1;
    double new_price;
    int64_t sum_volume;
};

/**
 * 防对敲
 */
class AntiSelfKnockRisker : public Risker {
 public:
    ~AntiSelfKnockRisker();

    void AddOption(std::shared_ptr<RiskOptions> opt);
    std::string GetAccountInfo(const std::string& fund_id);

    std::string HandleTradeOrderReq(MemTradeOrderMessage* req);
    void OnTradeOrderReqPass(MemTradeOrderMessage* req);

    void HandleTradeOrderRep(MemTradeOrderMessage* rep);

    std::string HandleTradeWithdrawReq(MemTradeWithdrawMessage* req);
    void HandleTradeWithdrawRep(MemTradeWithdrawMessage* rep);

    void OnTradeKnock(MemTradeKnock* knock);

    void OnTick(MemQTickBody* tick);

    void OnOrderFinish(OrderPtr order);

 private:
    AntiSelfKnockOption* GetOption(const std::string& fund_id);
    OrderBook* MustGetOrderBook(const std::string& code);
    OrderBook* TryGetOrderBook(const std::string& code);
    bool IsETF(const std::string& code);
    std::string CreateOrderNoKey(const std::string& fund_id, const std::string& order_no);

 private:
    std::unordered_map<std::string, AntiSelfKnockOption*> options_;  // fund_id -> option

    // 订单薄，委托结束后不一定会立即删除，最迟删除时机是在风控检查时；code -> OrderBook
    std::unordered_map<std::string, OrderBook*> order_books_;
    // 单笔委托，委托结束后立即删除 <fund_id>#<order_no> -> order
    std::unordered_map<std::string, OrderPtr> single_orders_;
    // 批量委托，委托全部结束后立即删除 <fund_id>#<batch_no> -> orders
    std::unordered_map<std::string, std::unique_ptr<std::vector<OrderPtr>>> batch_orders_;
    flatbuffers::FlatBufferBuilder fbb_;
    // 先收到成交回报, 后收到报单响应
    std::unordered_map<std::string, std::unique_ptr<std::vector<MemTradeKnock>>> knock_first_orders_;
};
}  // namespace co