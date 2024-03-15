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


namespace co {
    struct MemTradeAccount {
        char fund_id[64];
        int64_t timestamp = 0;
        char name[64];
        int64_t type = 0;
        int64_t batch_order_size = 0;
        bool disabled = false;
        bool enable_trader = false;
        bool enable_researcher = false;
        bool enable_risker = false;
        char market[128];
    };

    struct MemGetTradeAssetMessage {
        char id[64];
        int64_t timestamp = 0;
        char fund_id[64];
    };

    struct MemGetTradePositionMessage {
        char id[64];
        int64_t timestamp = 0;
        char fund_id[64];
        char cursor[128];
        int64_t items_size;
        char error[kMemErrorSize];
    };

    struct MemGetTradeKnockMessage {
        char id[64];
        int64_t timestamp = 0;
        char fund_id[64];
        char cursor[128];
        char next_cursor[128];
        int64_t items_size;
        char error[kMemErrorSize];
    };

    struct HeartBeatMessage {
        char fund_id[64];
        int64_t timestamp = 0;
    };

    constexpr int kMemTypeQueryTradeAssetReq = 6400001;
    constexpr int kMemTypeQueryTradeAssetRep = 6400002;
    constexpr int kMemTypeQueryTradePositionReq = 6400003;
    constexpr int kMemTypeQueryTradePositionRep = 6400004;
    constexpr int kMemTypeQueryTradeKnockReq = 6400005;
    constexpr int kMemTypeQueryTradeKnockRep = 6400006;
    constexpr int kMemTypeRtnTradeKnock = 6400007;
    constexpr int kMemTypeHeartBeat = 6400008;
}    // namespace co
