#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>

#include "x/x.h"
#include "coral/coral.h"
#include "mem_struct.h"
#include "inner_option_position.h"

namespace co {
class InnerOptionMaster {
 public:
    InnerOptionMaster() = default;

    // 清空持仓，用于断线重连后重新初始化
    void Clear();
    // 更新初始持仓
    void SetInitPositions(MemGetTradePositionMessage* rep);
    // 更新委托和成交
    void HandleOrderReq(const std::string& fund_id, int64_t bs_flag, const MemTradeOrder& order);
    void HandleOrderRep(const std::string& fund_id, int64_t bs_flag, const MemTradeOrder& order);
    void HandleKnock(const MemTradeKnock* knock);

   int64_t GetAutoOcFlag(const std::string& fund_id, int64_t bs_flag, const MemTradeOrder& order);

 protected:
    bool IsAccountInitialized(std::string fund_id);
    void Update(InnerOptionPositionPtr pos, int64_t oc_flag, int64_t order_volume, int64_t match_volume, int64_t withdraw_volume);
    std::string GetKey(std::string code, int64_t bs_flag);
    InnerOptionPositionPtr GetPosition(std::string fund_id, std::string code, int64_t bs_flag);
    InnerOptionPositionPtr GetOrCreatePosition(std::string fund_id, std::string code, int64_t bs_flag);

 private:
    std::map<std::string, std::shared_ptr<std::map<std::string, InnerOptionPositionPtr>>> positions_; // <fund_id> -> <code>_<bs_flag> -> obj
    std::unordered_map<std::string, bool> knocks_; // <fund_id>_<inner_match_no> -> true
};
    typedef std::shared_ptr<InnerOptionMaster> InnerOptionMasterPtr;
}  // namespace co
