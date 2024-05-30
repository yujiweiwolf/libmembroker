// Copyright 2021 Fancapital Inc.  All rights reserved.
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
#include "inner_stock_position.h"

namespace co {
class InnerStockMaster {
 public:
    InnerStockMaster();

    void Clear();

    void SetInitPositions(MemGetTradePositionMessage* rep);
    void HandleOrderReq(const std::string& fund_id, int64_t bs_flag, const MemTradeOrder& order);
    void HandleOrderRep(const std::string& fund_id, int64_t bs_flag, const MemTradeOrder& order);
    void HandleKnock(const MemTradeKnock* knock);

    int64_t GetOcFlag(const std::string& fund_id, int64_t bs_flag, const MemTradeOrder& order);

 protected:
    bool IsAccountInitialized(std::string fund_id);
    bool IsT0Type(const std::string& code);
    InnerStockPositionPtr GetOrCreatePosition(std::string fund_id, std::string code);

 private:
    std::map<std::string, std::shared_ptr<std::map<std::string, InnerStockPositionPtr>>> positions_;  // <fund_id> -> <code> -> obj
    std::unordered_map<std::string, bool> knocks_;  // <fund_id>_<inner_match_no> -> true
    std::set<std::string> t0_list_;
};
}  // namespace co



