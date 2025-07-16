// Copyright 2025 Fancapital Inc.  All rights reserved.
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
    InnerOptionPosition(std::string code, int64_t bs_flag) : code_(code) {
        if (bs_flag == kBsFlagBuy) {
            tag_ = "多头持仓";
        } else {
            tag_ = "空头持仓";
        }
    }

    int64_t GetAvailableVolume() {
        return (init_volume_ + open_volume_ - closing_volume_ - close_volume_);
    }

    std::string ToString() {
        std::stringstream ss;
        ss << "InnerPosition{";
        ss << "code: " << code_ << ", " << tag_
           << ", init_volume: " << init_volume_
           << ", closing_volume: " << closing_volume_
           << ", close_volume: " << close_volume_
           << ", opening_volume: " << opening_volume_
           << ", open_volume: " << open_volume_
           << "}";
        return ss.str();
    }

    std::string code_;
    std::string tag_;               // 多头持仓, 空头持仓
    int64_t init_volume_ = 0;       // 今天初始可用持仓， 一直不变
    int64_t closing_volume_ = 0;    // 今日持仓平仓冻结数
    int64_t close_volume_ = 0;      // 今日持仓已平仓数
    int64_t opening_volume_ = 0;    // 今日持仓开仓冻结数
    int64_t open_volume_ = 0;       // 今日持仓已开仓数
};
typedef std::shared_ptr<InnerOptionPosition> InnerOptionPositionPtr;

class InnerOptionMaster {
 public:
    InnerOptionMaster() = default;
    // 更新初始持仓
    void InitPositions(MemGetTradePositionMessage* rep);
    // 更新委托和成交
    void HandleOrderReq(int64_t bs_flag, const MemTradeOrder& order);
    void HandleOrderRep(int64_t bs_flag, const MemTradeOrder& order);
    void HandleKnock(const MemTradeKnock& knock);

    int64_t GetAutoOcFlag(int64_t bs_flag, const MemTradeOrder& order);
    InnerOptionPositionPtr GetPosition(std::string code, int64_t bs_flag);

 protected:
    bool IsAccountInitialized();
    void Update(InnerOptionPositionPtr pos, int64_t oc_flag, int64_t order_volume, int64_t match_volume, int64_t withdraw_volume);


 private:
    bool init_flag_ = false;
    std::map<std::string, std::shared_ptr<std::pair<InnerOptionPositionPtr, InnerOptionPositionPtr>>> positions_;  // <code> -> first is buy, second is sell
    std::set<std::string> knocks_;  // <inner_match_no>
};
typedef std::shared_ptr<InnerOptionMaster> InnerOptionMasterPtr;
}  // namespace co
