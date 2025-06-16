#pragma once

#include <string>

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

    void Fix(co::fbs::TradeAssetT* asset);
    void Fix(co::fbs::TradePositionT* position);
    void Fix(co::fbs::TradeKnockT* knock);
    void Validate(const co::fbs::TradeAssetT& asset);
    void Validate(const co::fbs::TradePositionT& position);
    void Validate(const co::fbs::TradeKnockT& knock);

	int64_t FixTimestamp(const int64_t& timestamp);

    std::string CreateHeartbeatMessage();

    std::string CreateFrame(const int64_t& function_id, const std::string& raw, char compress_algo);
    int64_t ParseFunctionIDFromFrame(const std::string& frame);

    void SetTraceReqBegin(flatbuffers::Vector<int64_t>* traces, int64_t ns);
    void SetTraceReqEnd(flatbuffers::Vector<int64_t>* traces, int64_t ns);
    void SetTraceRepBegin(flatbuffers::Vector<int64_t>* traces, int64_t ns);
    void SetTraceRepEnd(flatbuffers::Vector<int64_t>* traces, int64_t ns, int64_t* req, int64_t* rep, int64_t* out);

}