// Copyright 2025 Fancapital Inc.  All rights reserved.
#include "inner_stock_master.h"

namespace co {
    void InnerStockMaster::AddT0Code(const string& code) {
        t0_list_.insert(code);
        LOG_INFO << "T+0 code: " << code;
    }

    void InnerStockMaster::SetInitPositions(MemGetTradePositionMessage* rep) {
        LOG_INFO << "set init stock position";
        init_flag_ = true;
        if (rep->items_size > 0) {
            auto first = (MemTradePosition*)((char*)rep + sizeof(MemGetTradePositionMessage));
            for (int i = 0; i < rep->items_size; i++) {
                MemTradePosition *position = first + i;
                string code = position->code;
                InnerStockPositionPtr pos = GetPosition(code);
                // 普通卖的总额度
                pos->init_sell_volume_ = position->long_can_close;
                // 融券卖的总额度
                pos->init_borrowed_volume_ = position->short_can_open;
            }
        }
        LOG_INFO << "[AutoOpenClose] OnInit";
        for (auto it : positions_) {
            LOG_INFO << it.first << ", " << it.second->ToString();
        }
    }

    bool InnerStockMaster::IsT0Type(const std::string& code) {
        bool type_flag = false;
        if (t0_list_.find(code) != t0_list_.end()) {
            type_flag = true;
        }
        return type_flag;
    }

    void InnerStockMaster::HandleOrderReq(int64_t bs_flag, const MemTradeOrder& order) {
        // 处理委托请求，冻结数量
        std::string code = order.code;
        if (!IsAccountInitialized()) {
            return;
        }
        InnerStockPositionPtr pos = GetPosition(code);
        if (order.oc_flag == kOcFlagAuto && bs_flag == kBsFlagBuy) {
            // 委托类型：正常买入
            pos->buying_volume_ += order.volume;
        } else if (order.oc_flag == kOcFlagAuto && bs_flag == kBsFlagSell) {
            // 委托类型：正常卖出
            pos->selling_volume_ += order.volume;
        } else if (order.oc_flag == kOcFlagOpen && bs_flag == kBsFlagSell) {
            // 委托类型：融券卖出
            pos->borrowing_volume_ += order.volume;
        } else if (order.oc_flag == kOcFlagClose && bs_flag == kBsFlagBuy) {
            // 委托类型：买券还券
            pos->returning_volume_ += order.volume;
        }
        LOG_INFO << "HandleOrderReq, " << pos->ToString();
    }

    void InnerStockMaster::HandleOrderRep(int64_t bs_flag, const MemTradeOrder& order) {
        // 处理委托废单响应，解冻数量
        if (strlen(order.order_no) > 0) {
            return;
        }

        std::string code = order.code;
        if (!IsAccountInitialized()) {
            return;
        }
        InnerStockPositionPtr pos = GetPosition(code);
        if (order.oc_flag == kOcFlagAuto && bs_flag == kBsFlagBuy) {
            // 委托类型：正常买入
            if (order.volume <= pos->buying_volume_) {
                pos->buying_volume_ -= order.volume;
            }
        } else if (order.oc_flag == kOcFlagAuto && bs_flag == kBsFlagSell) {
            // 委托类型：正常卖出
            if (order.volume <= pos->selling_volume_) {
                pos->selling_volume_ -= order.volume;
            }
        } else if (order.oc_flag == kOcFlagOpen && bs_flag == kBsFlagSell) {
            // 委托类型：融券卖出
            if (order.volume <= pos->borrowing_volume_) {
                pos->borrowing_volume_ -= order.volume;
            }
        } else if (order.oc_flag == kOcFlagClose && bs_flag == kBsFlagBuy) {
            // 委托类型：买券还券
            if (order.volume <= pos->returning_volume_) {
                pos->returning_volume_ -= order.volume;
            }
        }
        LOG_INFO << "HandleOrderRep, " << pos->ToString();
    }

    void InnerStockMaster::HandleKnock(const MemTradeKnock& knock) {
        std::string fund_id = knock.fund_id;
        std::string inner_match_no = knock.inner_match_no;
        std::string code = knock.code;
        int64_t bs_flag = knock.bs_flag;
        int64_t oc_flag = knock.oc_flag;
        int64_t match_type = knock.match_type;
        if (fund_id.empty() || inner_match_no.empty() || code.empty()) {
            return;
        }
        if (bs_flag != kBsFlagBuy && bs_flag != kBsFlagSell) {
            return;
        }
        if (knock.match_volume <= 0) {
            return;
        }
        if (!IsAccountInitialized()) {
            return;
        }
        if (auto it = knocks_.find(inner_match_no); it != knocks_.end()) {
            return;
        }
        knocks_.insert(inner_match_no);
        int64_t match_volume = knock.match_volume;
        InnerStockPositionPtr pos = GetPosition(code);
        if (match_type == co::kMatchTypeOK) {
            if (oc_flag == kOcFlagAuto && bs_flag == kBsFlagBuy) {
                pos->bought_volume_ += match_volume;
                pos->buying_volume_ -= match_volume;
            } else if (oc_flag == kOcFlagAuto && bs_flag == kBsFlagSell) {
                pos->sold_volume_ += match_volume;
                pos->selling_volume_ -= match_volume;
            } else if (oc_flag == kOcFlagOpen && bs_flag == kBsFlagSell) {
                pos->borrowed_volume_ += match_volume;
                pos->borrowing_volume_ -= match_volume;
            } else if (oc_flag == kOcFlagClose && bs_flag == kBsFlagBuy) {
                pos->returned_volume_ += match_volume;
                pos->returning_volume_ -= match_volume;
            }
        } else if (match_type == co::kMatchTypeWithdrawOK || match_type == co::kMatchTypeFailed) {
            if (oc_flag == 0 && bs_flag == kBsFlagBuy) {
                pos->buying_volume_ -= match_volume;
            } else if (oc_flag == kOcFlagAuto && bs_flag == kBsFlagSell) {
                pos->selling_volume_ -= match_volume;
            } else if (oc_flag == kOcFlagOpen && bs_flag == kBsFlagSell) {
                pos->borrowing_volume_ -= match_volume;
            } else if (oc_flag == kOcFlagClose && bs_flag == kBsFlagBuy) {
                pos->returning_volume_ -= match_volume;
            }
        }
        LOG_INFO << "HandleKnock, " << pos->ToString();
    }

    int64_t InnerStockMaster::GetOcFlag(int64_t bs_flag, const MemTradeOrder& order) {
        if (bs_flag != kBsFlagSell) {
            return kOcFlagAuto;
        }

        int64_t oc_flag = kOcFlagAuto;
        if (!IsAccountInitialized()) {
            return kOcFlagAuto;  // 没有初始化该资金账号的持仓，直接返回普通卖出
        }

        InnerStockPositionPtr pos = GetPosition(order.code);
        bool t0_flag = IsT0Type(order.code);
        if (t0_flag) {
            // T + 0的品种， 当日买成交数据须统计
            if (order.volume <= (pos->bought_volume_ + pos->init_sell_volume_ - pos->selling_volume_ - pos->sold_volume_)) {
                oc_flag = kOcFlagAuto;
            } else {
                oc_flag = kOcFlagOpen;
            }
        } else {
            if (order.volume <= (pos->init_sell_volume_ - pos->selling_volume_ - pos->sold_volume_)) {
                oc_flag = kOcFlagAuto;
            } else {
                oc_flag = kOcFlagOpen;
            }
        }
        LOG_INFO << "[GetOcFlag]["
                 << order.code << ", bs_flag: " << bs_flag << ", order_volume: " << order.volume
                 << "] oc_flag: " << oc_flag << ", " << pos->ToString();
        return oc_flag;
    }

    bool InnerStockMaster::IsAccountInitialized() {
        return init_flag_;
    }

    InnerStockPositionPtr InnerStockMaster::GetPosition(std::string code) {
        InnerStockPositionPtr pos = nullptr;
        auto it = positions_.find(code);
        if (it != positions_.end()) {
            pos = it->second;
        } else {
            pos = std::make_shared<InnerStockPosition>(code);
            positions_[code] = pos;
        }
        return pos;
    }
}  // namespace co
