// Copyright 2021 Fancapital Inc.  All rights reserved.
#pragma once
#include <thread>
#include <map>
#include <string>
#include <memory>
#include <unordered_map>

#include "x/x.h"
#include "coral/coral.h"
#include "mem_struct.h"

namespace co {
struct InnerOptionPosition {
    InnerOptionPosition(std::string code, int64_t bs_flag) : code_(code), bs_flag_(bs_flag) {
    }

    std::string ToString() {
        std::stringstream ss;
        ss << "InnerPosition{";
        ss << "code: " << code_
           << ", bs_flag: " << bs_flag_
           << ", yd_volume: " << yd_volume_
           << ", yd_closing_volume: " << yd_closing_volume_
           << ", yd_close_volume: " << yd_close_volume_
           << ", td_volume: " << td_volume_
           << ", td_closing_volume: " << td_closing_volume_
           << ", td_close_volume: " << td_close_volume_
           << ", td_opening_volume: " << td_opening_volume_
           << ", td_open_volume: " << td_open_volume_
           << "}";
        return ss.str();
    }

    std::string code_;
    int64_t bs_flag_ = 0;  // 买卖标记：1-买入，2-卖出
    int64_t yd_volume_ = 0; // 昨日持仓  可用数量
    int64_t yd_closing_volume_ = 0; // 昨日持仓平仓冻结数
    int64_t yd_close_volume_ = 0; // 昨日持仓已平仓数
    int64_t td_volume_ = 0;  // 今日持仓
    int64_t td_closing_volume_ = 0;  // 今日持仓平仓冻结数
    int64_t td_close_volume_ = 0;  // 今日持仓已平仓数
    int64_t td_opening_volume_ = 0;  // 今日持仓开仓冻结数
    int64_t td_open_volume_ = 0;  // 今日持仓已开仓数
};
typedef std::shared_ptr<InnerOptionPosition> InnerOptionPositionPtr;
class InnerOptionMaster {
 public:
    InnerOptionMaster() = default;

    // 清空持仓，用于断线重连后重新初始化
    void Clear();
    // 更新初始持仓
    void SetInitPositions(MemGetTradePositionMessage* rep);
    // 更新委托和成交
    void HandleOrderReq(int64_t bs_flag, const MemTradeOrder& order);
    void HandleOrderRep(int64_t bs_flag, const MemTradeOrder& order);
    void HandleKnock(const MemTradeKnock* knock);

    int64_t GetAutoOcFlag(int64_t bs_flag, const MemTradeOrder& order);

 protected:
    bool IsAccountInitialized();
    void Update(InnerOptionPositionPtr pos, int64_t oc_flag, int64_t order_volume, int64_t match_volume, int64_t withdraw_volume);
    std::string GetKey(std::string code, int64_t bs_flag);
    InnerOptionPositionPtr GetPosition(std::string code, int64_t bs_flag);
    InnerOptionPositionPtr GetOrCreatePosition(std::string code, int64_t bs_flag);

 private:
    std::map<std::string, std::shared_ptr<std::pair<InnerOptionPositionPtr, InnerOptionPositionPtr>>> positions_;  // <code> -> first is buy, second is sell
    std::set<std::string> knocks_;  // <inner_match_no>
};
    typedef std::shared_ptr<InnerOptionMaster> InnerOptionMasterPtr;
}  // namespace co
