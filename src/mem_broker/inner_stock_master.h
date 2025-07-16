// Copyright 2025 Fancapital Inc.  All rights reserved.
#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <memory>

#include "x/x.h"
#include "coral/coral.h"
#include "mem_struct.h"

using std::string;

namespace co {
struct InnerStockPosition {
    InnerStockPosition(std::string code) : code_(code) {
    }

    string ToString() {
        std::stringstream ss;
        ss << "InnerPosition{";
        ss << "code: " << code_
           << ", total_borrowed_volume: " << init_borrowed_volume_
           << ", borrowed_volume: " << borrowed_volume_
           << ", borrowing_volume: " << borrowing_volume_
           << ", returned_volume: " << returned_volume_
           << ", returning_volume: " << returning_volume_
           << ", init_sell_volume_: " << init_sell_volume_
           << ", bought_volume: " << bought_volume_
           << ", buying_volume: " << buying_volume_
           << ", sold_volume: " << sold_volume_
           << ", selling_volume: " << selling_volume_
           << "}";
        return ss.str();
    }

    std::string code_;
    int64_t init_borrowed_volume_ = 0;   // 今日融券卖出的总额度, 不会变化
    int64_t borrowed_volume_ = 0;        // 已融券卖出数量
    int64_t borrowing_volume_ = 0;       // 融券卖出冻结数量
    int64_t returned_volume_ = 0;        // 已买券还券数量
    int64_t returning_volume_ = 0;       // 买券还券冻结数量

    int64_t init_sell_volume_ = 0;       // 今日普通卖出的总额度, 不会变化
    int64_t bought_volume_ = 0;          // 普通已买入数量
    int64_t buying_volume_ = 0;          // 普通买入冻结数量
    int64_t sold_volume_ = 0;            // 普通已卖出数量
    int64_t selling_volume_ = 0;         // 普通卖出冻结数量
};
typedef std::shared_ptr<InnerStockPosition> InnerStockPositionPtr;

class InnerStockMaster {
 public:
    InnerStockMaster() = default;
    void AddT0Code(const string& code);
    void SetInitPositions(MemGetTradePositionMessage* rep);
    void HandleOrderReq(int64_t bs_flag, const MemTradeOrder& order);
    void HandleOrderRep(int64_t bs_flag, const MemTradeOrder& order);
    void HandleKnock(const MemTradeKnock& knock);

    int64_t GetOcFlag(int64_t bs_flag, const MemTradeOrder& order);
    InnerStockPositionPtr GetPosition(std::string code);

private:
    bool IsAccountInitialized();
    bool IsT0Type(const std::string& code);

 private:
    bool init_flag_ = false;
    std::set<std::string> knocks_;
    std::set<std::string> t0_list_;
    std::map<std::string, InnerStockPositionPtr> positions_;  // key is code
};
}  // namespace co



