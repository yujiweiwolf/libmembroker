// Copyright 2021 Fancapital Inc.  All rights reserved.
#pragma once
#include "x/x.h"
#include "coral/coral.h"

//  约定共享内存的大小
constexpr int64_t kReqMemSize = 64;
constexpr int64_t kRepMemSize = 64;

constexpr int kMemTypeQueryTradeAssetReq = 6400001;
constexpr int kMemTypeQueryTradeAssetRep = 6400002;
constexpr int kMemTypeQueryTradePositionReq = 6400003;
constexpr int kMemTypeQueryTradePositionRep = 6400004;
constexpr int kMemTypeQueryTradeKnockReq = 6400005;
constexpr int kMemTypeQueryTradeKnockRep = 6400006;
constexpr int kMemTypeInnerCyclicSignal = 6400007;
constexpr int kMemTypeHeartBeat = 6400008;
constexpr int kMemTypeMonitorRisk = 6400009;

namespace co {
struct MemTradeAccount {
    char fund_id[kMemFundIdSize];
    int64_t timestamp = 0;
    char name[128];
    int64_t type = 0;
    int64_t batch_order_size = 0;
//    bool disabled = false;
//    bool enable_trader = false;
//    bool enable_researcher = false;
//    bool enable_risker = false;
//    int64_t market;
};

struct MemGetTradeAssetMessage {
    char id[kMemIdSize];
    int64_t timestamp = 0;
    char fund_id[kMemFundIdSize];
    int64_t items_size;
    char error[kMemErrorSize];
    MemTradeAsset items[];
};

struct MemGetTradePositionMessage {
    char id[kMemIdSize];
    int64_t timestamp = 0;
    char fund_id[kMemFundIdSize];
    char cursor[128];
    int64_t items_size;
    char error[kMemErrorSize];
    MemTradePosition items[];
};

struct MemGetTradeKnockMessage {
    char id[kMemIdSize];
    int64_t timestamp = 0;
    char fund_id[kMemFundIdSize];
    char cursor[128];
    char next_cursor[128];
    int64_t items_size;
    char error[kMemErrorSize];
    MemTradeKnock items[];
};

struct HeartBeatMessage {
    char fund_id[kMemFundIdSize];
    int64_t timestamp = 0;
};

struct MemMonitorRiskMessage {
    int64_t timestamp = 0;
    char error[1024];
};
}  // namespace co

