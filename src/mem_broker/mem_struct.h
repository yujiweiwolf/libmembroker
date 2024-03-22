#pragma once

#include <string>
#include <queue>
#include <vector>
#include <map>
#include <list>
#include <deque>
#include <memory>
#include <tuple>
#include <boost/asio.hpp>
#include "coral/coral.h"

//  约定共享内存的大小
constexpr int64_t kReqMemSize = 64;
constexpr int64_t kRepMemSize = 64;
constexpr int64_t kInnerBrokerMemSize = 8;
const char kInnerBrokerFile[] = "inner_broker";

namespace co {
    struct MemTradeAccount {
        char fund_id[kMemFundIdSize]; // 资金账号
        int64_t timestamp = 0;
        char name[128];
        int64_t type = 0;
        int64_t batch_order_size = 0;
        bool disabled = false;
        bool enable_trader = false;
        bool enable_researcher = false;
        bool enable_risker = false;
        int64_t market;
    };

    struct MemGetTradeAssetMessage {
        char id[kMemIdSize]; // 消息编号
        int64_t timestamp = 0;
        char fund_id[kMemFundIdSize]; // 资金账号
        int64_t items_size;
        char error[kMemErrorSize];
    };

    struct MemGetTradePositionMessage {
        char id[kMemIdSize]; // 消息编号
        int64_t timestamp = 0;
        char fund_id[kMemFundIdSize]; // 资金账号
        char cursor[128];
        int64_t items_size;
        char error[kMemErrorSize];
    };

    struct MemGetTradeKnockMessage {
        char id[kMemIdSize]; // 消息编号
        int64_t timestamp = 0;
        char fund_id[kMemFundIdSize]; // 资金账号
        char cursor[128];
        char next_cursor[128];
        int64_t items_size;
        char error[kMemErrorSize];
    };

    struct HeartBeatMessage {
        char fund_id[kMemFundIdSize]; // 资金账号
        int64_t timestamp = 0;
    };

    constexpr int kMemTypeQueryTradeAssetReq = 6400001;
    constexpr int kMemTypeQueryTradeAssetRep = 6400002;
    constexpr int kMemTypeQueryTradePositionReq = 6400003;
    constexpr int kMemTypeQueryTradePositionRep = 6400004;
    constexpr int kMemTypeQueryTradeKnockReq = 6400005;
    constexpr int kMemTypeQueryTradeKnockRep = 6400006;
    constexpr int kMemTypeInnerCyclicSignal = 6400007;
    constexpr int kMemTypeHeartBeat = 6400008;
}    // namespace co
