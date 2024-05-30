// Copyright 2021 Fancapital Inc.  All rights reserved.
#include "inner_stock_master.h"

namespace co {
    InnerStockMaster::InnerStockMaster() {
    }

    void InnerStockMaster::Clear() {
        positions_.clear();
    }

    // T+0的合约，type须设置为1， 不管其有没有持仓
    void InnerStockMaster::SetInitPositions(MemGetTradePositionMessage* rep) {
        LOG_INFO << "set init stock position";
        std::string fund_id = rep->fund_id;
        auto all = std::make_shared<std::map<std::string, InnerStockPositionPtr>>();
        positions_[fund_id] = all;
        if (rep->items_size > 0) {
            auto first = (MemTradePosition*)((char*)rep + sizeof(MemGetTradePositionMessage));
            for (int i = 0; i < rep->items_size; i++) {
                MemTradePosition *position = first + i;
                string code = position->code;
                int64_t long_can_close = position->long_can_close;
                int64_t short_can_open = position->short_can_open;
                int64_t type = position->type;  // 默认值是0， 1 表示 T + 0
                // 普通卖
                if (long_can_close > 0) {
                    InnerStockPositionPtr pos = GetOrCreatePosition(fund_id, code);
                    pos->set_total_sell_volume(long_can_close);
                }
                // 融券卖
                if (short_can_open > 0) {
                    InnerStockPositionPtr pos = GetOrCreatePosition(fund_id, code);
                    pos->set_total_borrowed_volume(short_can_open);
                }
                if (type == 1) {
                    LOG_INFO << "T+0 code: " << code;
                    t0_list_.insert(code);
                }
            }
        }
    }

    bool InnerStockMaster::IsT0Type(const std::string& code) {
        bool type_flag = false;
        if (t0_list_.find(code) != t0_list_.end()) {
            type_flag = true;
        }
        return type_flag;
    }

    void InnerStockMaster::HandleOrderReq(const std::string& fund_id, int64_t bs_flag, const MemTradeOrder& order) {
        // 处理委托请求，冻结数量
        std::string code = order.code;
        if (!IsAccountInitialized(fund_id)) {
            return;
        }
        InnerStockPositionPtr pos = GetOrCreatePosition(fund_id, code);
        if (order.oc_flag == kOcFlagAuto && bs_flag == kBsFlagBuy) {
            // 委托类型：正常买入
            pos->set_buying_volume(pos->buying_volume() + order.volume);
        } else if (order.oc_flag == kOcFlagAuto && bs_flag == kBsFlagSell) {
            // 委托类型：正常卖出
            pos->set_selling_volume(pos->selling_volume() + order.volume);
        } else if (order.oc_flag == kOcFlagOpen && bs_flag == kBsFlagSell) {
            // 委托类型：融券卖出
            pos->set_borrowing_volume(pos->borrowing_volume() + order.volume);
        } else if (order.oc_flag == kOcFlagClose && bs_flag == kBsFlagBuy) {
            // 委托类型：买券还券
            pos->set_returning_volume(pos->returning_volume() + order.volume);
        }
        LOG_INFO << __FUNCTION__ << "  " << pos->ToString();
    }

    void InnerStockMaster::HandleOrderRep(const std::string& fund_id, int64_t bs_flag, const MemTradeOrder& order) {
        // 处理委托废单响应，解冻数量
        if (order.state != kOrderFailed) {
            return;
        }

        std::string code = order.code;
        if (!IsAccountInitialized(fund_id)) {
            return;
        }
        InnerStockPositionPtr pos = GetOrCreatePosition(fund_id, code);
        if (order.oc_flag == kOcFlagAuto && bs_flag == kBsFlagBuy) {
            // 委托类型：正常买入
            if (order.volume <= pos->buying_volume()) {
                pos->set_buying_volume(pos->buying_volume() - order.volume);
            }
        } else if (order.oc_flag == kOcFlagAuto && bs_flag == kBsFlagSell) {
            // 委托类型：正常卖出
            if (order.volume <= pos->selling_volume()) {
                pos->set_selling_volume(pos->selling_volume() - order.volume);
            }
        } else if (order.oc_flag == kOcFlagOpen && bs_flag == kBsFlagSell) {
            // 委托类型：融券卖出
            if (order.volume <= pos->borrowing_volume()) {
                pos->set_borrowing_volume(pos->borrowing_volume() - order.volume);
            }
        } else if (order.oc_flag == kOcFlagClose && bs_flag == kBsFlagBuy) {
            // 委托类型：买券还券
            if (order.volume <= pos->returning_volume()) {
                pos->set_returning_volume(pos->returning_volume() - order.volume);
            }
        }
        LOG_INFO << __FUNCTION__ << "  " << pos->ToString();
    }

    void InnerStockMaster::HandleKnock(const MemTradeKnock* knock) {
        std::string fund_id = knock->fund_id;
        std::string inner_match_no = knock->inner_match_no;
        std::string code = knock->code;
        int64_t bs_flag = knock->bs_flag;
        int64_t oc_flag = knock->oc_flag;
        int64_t match_type = knock->match_type;
        if (fund_id.empty() || inner_match_no.empty() || code.empty()) {
            return;
        }
        if (bs_flag != kBsFlagBuy && bs_flag != kBsFlagSell) {
            return;
        }
        if (knock->match_volume <= 0) {
            return;
        }
        if (!IsAccountInitialized(fund_id)) {
            return;  // 没有初始化该资金账号的持仓，忽略
        }
        std::stringstream ss;
        ss << fund_id << "#" << inner_match_no;
        std::string key = ss.str();
        auto itr = knocks_.find(key);
        if (itr != knocks_.end()) {
            return;  // 该成交回报已处理过，忽略
        }
        knocks_[key] = true;
        int64_t match_volume = knock->match_volume;
        InnerStockPositionPtr pos = GetOrCreatePosition(fund_id, code);
        if (match_type == co::kMatchTypeOK) {
            if (oc_flag == kOcFlagAuto && bs_flag == kBsFlagBuy) {
                pos->set_bought_volume(pos->bought_volume() + match_volume);
                pos->set_buying_volume(pos->buying_volume() - match_volume);
            } else if (oc_flag == kOcFlagAuto && bs_flag == kBsFlagSell) {
                pos->set_sold_volume(pos->sold_volume() + match_volume);
                pos->set_selling_volume(pos->selling_volume() - match_volume);
            } else if (oc_flag == kOcFlagOpen && bs_flag == kBsFlagSell) {
                pos->set_borrowed_volume(pos->borrowed_volume() + match_volume);
                pos->set_borrowing_volume(pos->borrowing_volume() - match_volume);
            } else if (oc_flag == kOcFlagClose && bs_flag == kBsFlagBuy) {
                pos->set_returned_volume(pos->returned_volume() + match_volume);
                pos->set_returning_volume(pos->returning_volume() - match_volume);
            }
        } else if (match_type == co::kMatchTypeWithdrawOK || match_type == co::kMatchTypeFailed) {
            if (oc_flag == 0 && bs_flag == kBsFlagBuy) {
                pos->set_buying_volume(pos->buying_volume() - match_volume);
            } else if (oc_flag == kOcFlagAuto && bs_flag == kBsFlagSell) {
                pos->set_selling_volume(pos->selling_volume() - match_volume);
            } else if (oc_flag == kOcFlagOpen && bs_flag == kBsFlagSell) {
                pos->set_borrowing_volume(pos->borrowing_volume() - match_volume);
            } else if (oc_flag == kOcFlagClose && bs_flag == kBsFlagBuy) {
                pos->set_returning_volume(pos->returning_volume() - match_volume);
            }
        }
        LOG_INFO << __FUNCTION__ << "  " << pos->ToString();
    }

    int64_t InnerStockMaster::GetOcFlag(const std::string& fund_id, int64_t bs_flag, const MemTradeOrder& order) {
        if (bs_flag != kBsFlagSell) {
            return kOcFlagAuto;
        }

        int64_t oc_flag = kOcFlagAuto;
        if (!IsAccountInitialized(fund_id)) {
            return kOcFlagAuto;  // 没有初始化该资金账号的持仓，直接返回普通卖出
        }

        InnerStockPositionPtr pos = GetOrCreatePosition(fund_id, order.code);
        bool t0_flag = IsT0Type(order.code);
        if (t0_flag) {
            // T + 0的品种， 当日买成交数据须统计
            if (order.volume <= (pos->bought_volume() + pos->total_sell_volume() - pos->selling_volume() - pos->sold_volume())) {
                oc_flag = kOcFlagAuto;
            } else {
                oc_flag = kOcFlagOpen;
            }
        } else {
            if (order.volume <= (pos->total_sell_volume() - pos->selling_volume() - pos->sold_volume())) {
                oc_flag = kOcFlagAuto;
            } else {
                oc_flag = kOcFlagOpen;
            }
        }
        LOG_INFO << __FUNCTION__ << ", oc_flag: " << oc_flag << ", " << pos->ToString();
        return oc_flag;
    }

    bool InnerStockMaster::IsAccountInitialized(std::string fund_id) {
        return positions_.find(fund_id) != positions_.end() ? true : false;
    }

    InnerStockPositionPtr InnerStockMaster::GetOrCreatePosition(std::string fund_id, std::string code) {
        InnerStockPositionPtr pos = nullptr;
        std::shared_ptr<std::map<std::string, InnerStockPositionPtr>> all = nullptr;
        auto itr_acc = positions_.find(fund_id);
        if (itr_acc != positions_.end()) {
            all = itr_acc->second;
        } else {
            all = std::make_shared<std::map<std::string, InnerStockPositionPtr>>();
            positions_[fund_id] = all;
        }
        auto itr = all->find(code);
        if (itr != all->end()) {
            pos = itr->second;
        } else {
            pos = make_shared<InnerStockPosition>(fund_id, code);
            (*all)[code] = pos;
        }
        return pos;
    }
}  // namespace co
