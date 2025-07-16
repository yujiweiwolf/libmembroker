#pragma once

#include <string>
#include <random>
#include <algorithm>
#include "x/x.h"
#include "coral/coral.h"

namespace co {

    constexpr int64_t kMaxBodyBytes = 64 << 20; // 最大网络消息大小，64MB

    std::string SafeGBKToUTF8(const std::string& str);
    std::string DropCodeSuffix(const std::string& code);
    std::string AddCodeSuffix(const std::string& code, const int64_t& market);

    std::string CreateStandardOrderNo(int64_t market, const std::string_view& order_no);
    std::string ParseStandardOrderNo(const std::string_view& order_no, int64_t* market = nullptr);
    std::string CreateStandardBatchNo(int64_t market, int64_t batch_size, const std::string_view& batch_no);
    std::string ParseStandardBatchNo(const std::string_view& batch_no, int64_t* market = nullptr, int64_t* batch_size = nullptr);

    std::string CreateInnerOrderNo(const co::fbs::TradeOrderT& order);
    std::string CreateInnerMatchNo(const co::fbs::TradeKnockT& knock);
    int64_t CreateOrderStatus(const co::fbs::TradeOrderT& order);
    std::string CheckTradeOrderMessage(MemTradeOrderMessage *req, int sh_th_tps_limit, int sz_th_tps_limit);
    std::string CheckTradeWithdrawMessage(MemTradeWithdrawMessage *req, int64_t trade_type);

    std::string GenerateRandomString(size_t length);

}