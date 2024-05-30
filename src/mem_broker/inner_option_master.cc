// Copyright 2021 Fancapital Inc.  All rights reserved.
#include "inner_option_master.h"

namespace co {

    void InnerOptionMaster::Clear() {
        positions_.clear();
    }

    void InnerOptionMaster::SetInitPositions(MemGetTradePositionMessage* rep) {
        // 初始化持仓，这里没有使用昨持仓，只取今持仓
        LOG_INFO << "set init option position";
        std::string fund_id = rep->fund_id;
        if (fund_id.empty()) {
            return;
        }
        auto all = std::make_shared<std::map<std::string, InnerOptionPositionPtr>>();
        positions_[fund_id] = all;
        if (rep->items_size > 0) {
            string fund_id = rep->fund_id;
            auto first = (MemTradePosition*)((char*)rep + sizeof(MemGetTradePositionMessage));
            for (int i = 0; i < rep->items_size; i++) {
                MemTradePosition *position = first + i;
                string code = position->code;
                int64_t long_volume = position->long_can_close;
                int64_t short_volume = position->short_can_open;
                if (long_volume > 0) {
                    InnerOptionPositionPtr pos = GetOrCreatePosition(fund_id, code, kBsFlagBuy);
                    pos->set_td_volume(long_volume);
                }
                if (short_volume > 0) {
                    InnerOptionPositionPtr pos = GetOrCreatePosition(fund_id, code, kBsFlagSell);
                    pos->set_td_volume(short_volume);
                }
            }
        }
        int i = 0;
        std::stringstream ss_log;
        ss_log << "[AutoOpenClose] OnInit";
        for (auto itr : *all) {
            ++i;
            auto pos = itr.second;
            ss_log << std::endl << "[" << i << "/" << all->size() << "]"
                << "[AutoOpenClose]["
                << fund_id << "][" << pos->code() << "#" << pos->bs_flag()\
                << "] position = " << pos->ToString();
        }
        LOG_INFO << ss_log.str();
    }

    void InnerOptionMaster::HandleOrderReq(const std::string& fund_id, int64_t bs_flag, const MemTradeOrder& order) {
        // 处理委托请求，冻结数量
        std::string code = order.code;
        int64_t oc_flag = order.oc_flag;
        if (bs_flag != kBsFlagBuy && bs_flag != kBsFlagSell) {
            return;
        }
        if (oc_flag != kOcFlagOpen && oc_flag != kOcFlagClose) {
            return;
        }
        if (!IsAccountInitialized(fund_id)) {
            return;  // 没有初始化该资金账号的持仓，忽略
        }
        // -------------------------------------------------------------------
        // 获取待更新的持仓
        InnerOptionPositionPtr pos = nullptr;
        if ((bs_flag == kBsFlagBuy && oc_flag == kOcFlagOpen) || (bs_flag == kBsFlagSell && oc_flag == kOcFlagClose)) {
            // 买开和卖平（更新买持仓）
            pos = GetOrCreatePosition(fund_id, code, kBsFlagBuy);
        } else if ((bs_flag == kBsFlagSell && oc_flag == kOcFlagOpen) || (bs_flag == kBsFlagBuy && oc_flag == kOcFlagClose)) {
            // 卖开和买平（更新卖持仓）
            pos = GetOrCreatePosition(fund_id, code, kBsFlagSell);
        }
        if (pos) {
            std::string before = pos->ToString();
            Update(pos, oc_flag, order.volume, 0, 0);
            std::string after = pos->ToString();
            LOG_INFO << "[AutoOpenClose][" << fund_id << "]["
                << code << "#" << bs_flag << "] OnOrderReq: "
                << "oc_flag=" << oc_flag
                << ", order_volume=" << order.volume
                << ", before=" << before << ", after = " << after;
        }
    }

    void InnerOptionMaster::HandleOrderRep(const std::string& fund_id, int64_t bs_flag, const MemTradeOrder& order) {
        // 处理废单委托响应，解冻数量
        if (order.state != kOrderFailed) {
            return;
        }
        std::string code = order.code;
        int64_t oc_flag = order.oc_flag;
        if (bs_flag != kBsFlagBuy && bs_flag != kBsFlagSell) {
            return;
        }
        if (oc_flag != kOcFlagOpen && oc_flag != kOcFlagClose) {
            return;
        }
        if (!IsAccountInitialized(fund_id)) {
            return;  // 没有初始化该资金账号的持仓，忽略
        }
        // -------------------------------------------------------------------
        // 获取待更新的持仓
        InnerOptionPositionPtr pos = nullptr;
        if ((bs_flag == kBsFlagBuy && oc_flag == kOcFlagOpen) || (bs_flag == kBsFlagSell && oc_flag == kOcFlagClose)) {
            // 买开和卖平（更新买持仓）
            pos = GetOrCreatePosition(fund_id, code, kBsFlagBuy);
        } else if ((bs_flag == kBsFlagSell && oc_flag == kOcFlagOpen) || (bs_flag == kBsFlagBuy && oc_flag == kOcFlagClose)) {
            // 卖开和买平（更新卖持仓）
            pos = GetOrCreatePosition(fund_id, code, kBsFlagSell);
        }
        if (pos) {
            std::string before = pos->ToString();
            Update(pos, oc_flag, 0, 0, order.volume);
            std::string after = pos->ToString();
            LOG_INFO << "[AutoOpenClose][" << fund_id << "]["
                << code << "#" << bs_flag << "] OnOrderRep: "
                << "oc_flag=" << oc_flag
                << ", withdraw_volume=" << order.volume
                << ", before=" << before << ", after = " << after;
        }
    }

    void InnerOptionMaster::HandleKnock(const MemTradeKnock* knock) {
        // 更新内部持仓
        std::string fund_id = knock->fund_id;
        std::string inner_match_no = knock->inner_match_no;
        std::string code = knock->code;
        int64_t bs_flag = knock->bs_flag;
        int64_t oc_flag = knock->oc_flag;
        if (fund_id.empty() || inner_match_no.empty() || code.empty()) {
            return;
        }
        if (bs_flag != kBsFlagBuy && bs_flag != kBsFlagSell) {
            return;
        }
        if (oc_flag != kOcFlagOpen && oc_flag != kOcFlagClose) {
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
        int64_t match_volume = 0;
        int64_t withdraw_volume = 0;
        if (knock->match_type == kMatchTypeOK) {
            match_volume = knock->match_volume;
        } else if (knock->match_type == kMatchTypeWithdrawOK ||
            knock->match_type == kMatchTypeFailed) {
            withdraw_volume = knock->match_volume;
        }
        InnerOptionPositionPtr pos = nullptr;
        if ((bs_flag == kBsFlagBuy && oc_flag == kOcFlagOpen) || (bs_flag == kBsFlagSell && oc_flag == kOcFlagClose)) {
            // 买开和卖平（更新买持仓）
            pos = GetOrCreatePosition(fund_id, code, kBsFlagBuy);
        } else if ((bs_flag == kBsFlagSell && oc_flag == kOcFlagOpen) || (bs_flag == kBsFlagBuy && oc_flag == kOcFlagClose)) {
            // 卖开和买平（更新卖持仓）
            pos = GetOrCreatePosition(fund_id, code, kBsFlagSell);
        }
        if (pos && (match_volume > 0 || withdraw_volume > 0)) {
            std::string before = pos->ToString();
            Update(pos, oc_flag, 0, match_volume, withdraw_volume);
            std::string after = pos->ToString();
            LOG_INFO << "[AutoOpenClose][" << fund_id << "]["
                << code << "#" << bs_flag << "] OnKnock: "
                << "oc_flag=" << oc_flag
                << ", match_volume=" << match_volume
                << ", withdraw_volume=" << withdraw_volume
                << ", before=" << before << ", after = " << after;
        }
    }

    void InnerOptionMaster::Update(InnerOptionPositionPtr pos,
        int64_t oc_flag,
        int64_t order_volume,
        int64_t match_volume,
        int64_t withdraw_volume) {
        if (!pos) {
            return;
        }
        // 内部持仓更新逻辑
        // 1.买开（更新买持仓）
        // 1.1 买开委托：增加开仓冻结；
        // 1.2 买开成交：减少开仓冻结，增加持仓，增加已开仓数；
        // 1.3 买开撤单：减少开仓冻结；
        // 2.买平（更新卖持仓）
        // 2.1 买平委托：减少持仓，增加平仓冻结；
        // 2.2 买平成交：减少平仓冻结，增加已平仓数；
        // 2.3 买平撤单：减少平仓冻结，增加持仓；
        // 3.卖开（更新卖持仓）
        // 3.1 卖开委托：增加开仓冻结；
        // 3.2 卖开成交：减少开仓冻结，增加持仓、增加已开仓数；
        // 3.3 卖开撤单：减少开仓冻结；
        // 4.卖平（更新买持仓）
        // 4.1 卖平委托：减少持仓，增加平仓冻结；
        // 4.2 卖平成交：减少平仓冻结，增加已平仓数；
        // 4.3 卖平撤单：减少平仓冻结，增加持仓；
        switch (oc_flag) {
        case kOcFlagOpen:
            if (order_volume > 0) {  // 开仓委托：增加开仓冻结
                pos->set_td_opening_volume(pos->td_opening_volume() + order_volume);
            }
            if (match_volume > 0) {  // 开仓成交：减少开仓冻结，增加持仓，增加已开仓数
                pos->set_td_opening_volume(pos->td_opening_volume() - match_volume);
                pos->set_td_volume(pos->td_volume() + match_volume);
                pos->set_td_open_volume(pos->td_open_volume() + match_volume);
            }
            if (withdraw_volume > 0) {  // 开仓撤单：减少开仓冻结
                pos->set_td_opening_volume(pos->td_opening_volume() - withdraw_volume);
            }
            break;
        case kOcFlagClose:  // 平仓
            // 先平昨后平今
            if (order_volume > 0) {  // 平仓委托：减少持仓，增加平仓冻结
                int64_t yd = pos->yd_volume() >= order_volume ? order_volume : pos->yd_volume();  // 平昨委托数量
                int64_t td = yd < order_volume ? order_volume - yd : 0;  // 平今委托数量
                if (yd > 0) {
                    pos->set_yd_closing_volume(pos->yd_closing_volume() + yd);
                    pos->set_yd_volume(pos->yd_volume() - yd);
                }
                if (td > 0) {
                    int64_t td_volume = pos->td_volume() - td;
                    int64_t td_closing_volume = pos->td_closing_volume() + td;
                    pos->set_td_closing_volume(td_closing_volume);
                    pos->set_td_volume(td_volume);
                }
            }
            if (match_volume > 0) {  // 平仓成交：减少平仓冻结，增加已平仓数
                int64_t yd = pos->yd_closing_volume() >= match_volume ? match_volume : pos->yd_closing_volume();
                int64_t td = yd < match_volume ? match_volume - yd : 0;
                if (yd > 0) {
                    int64_t yd_closing_volume = pos->yd_closing_volume() - yd;
                    int64_t yd_close_volume = pos->yd_close_volume() + yd;
                    pos->set_yd_closing_volume(yd_closing_volume);
                    pos->set_yd_close_volume(yd_close_volume);
                }
                if (td > 0) {
                    int64_t td_closing_volume = pos->td_closing_volume() - td;
                    int64_t td_close_volume = pos->td_close_volume() + td;
                    pos->set_td_closing_volume(td_closing_volume);
                    pos->set_td_close_volume(td_close_volume);
                }
            }
            if (withdraw_volume > 0) {  // 平仓撤单：减少平仓冻结，增加持仓
                int64_t td = pos->td_closing_volume() >= withdraw_volume ? withdraw_volume : pos->td_closing_volume();
                int64_t yd = td < withdraw_volume ? withdraw_volume - td : 0;
                if (td > 0) {
                    int64_t td_closing_volume = pos->td_closing_volume() - td;
                    int64_t td_volume = pos->td_volume() + td;
                    pos->set_td_closing_volume(td_closing_volume);
                    pos->set_td_volume(td_volume);
                }
                if (yd > 0) {
                    int64_t yd_closing_volume = pos->yd_closing_volume() - yd;
                    int64_t yd_volume = pos->yd_volume() + yd;
                    pos->set_yd_closing_volume(yd_closing_volume);
                    pos->set_yd_volume(yd_volume);
                }
            }
            break;
        default:
            break;
        }
    }

    int64_t InnerOptionMaster::GetAutoOcFlag(const std::string& fund_id, int64_t bs_flag, const MemTradeOrder& order) {
        // 买开（bs_flag=买，oc_flag=自动）:
        // 1.如果有卖方向头寸，则执行：买平；
        // 2.如果没有卖方向头寸或卖方向头寸不足，则执行：买开
        // 卖开（bs_flag=买，oc_flag=自动）：
        // 1.如果有买方向头寸，则执行：卖平;
        // 2.如果没有买方向头寸或买方向头寸不足，则执行：卖开
        if (order.oc_flag != co::kOcFlagAuto) {  // 不是自动开平仓，直接返回请求中设定的开平仓标记
            return order.oc_flag;
        }
        int64_t ret_oc_flag = kOcFlagOpen;  // 默认开仓
        string code = order.code;
        int64_t order_volume = order.volume;
        if (bs_flag != kBsFlagBuy && bs_flag != kBsFlagSell) {
            return ret_oc_flag;
        }
        int64_t r_bs_flag = bs_flag == kBsFlagBuy ? kBsFlagSell : kBsFlagBuy;
        auto itr_acc = positions_.find(fund_id);
        if (itr_acc == positions_.end()) {  // 没有初始化该资金账号的持仓，直接返回开仓
            return ret_oc_flag;
        }
        auto positions = itr_acc->second;
        std::string key = GetKey(code, r_bs_flag);
        auto itr = positions->find(key);
        if (itr == positions->end()) {  // 没有持仓，直接返回开仓
            return ret_oc_flag;
        }
        auto pos = itr->second;
        // 上期所，平仓时需要指定是平今仓还是昨仓；
        // 其他交易所，平仓时不指定是平今仓还是昨仓，交易所自动以“先开先平”的原则进行处理。
        if (pos->yd_volume() >= order_volume) {  // 先平昨仓
            ret_oc_flag = kOcFlagClose;
        } else if (pos->td_volume() >= order_volume) {  // 后平今仓
            ret_oc_flag = kOcFlagClose;
        }
        return ret_oc_flag;
    }

    std::string InnerOptionMaster::GetKey(string code, int64_t bs_flag) {
        std::stringstream ss;
        ss << code << "_" << bs_flag;
        return ss.str();
    }

    bool InnerOptionMaster::IsAccountInitialized(std::string fund_id) {
        return positions_.find(fund_id) != positions_.end() ? true : false;
    }

    InnerOptionPositionPtr InnerOptionMaster::GetPosition(std::string fund_id, std::string code, int64_t bs_flag) {
        InnerOptionPositionPtr pos = nullptr;
        std::shared_ptr<std::map<std::string, InnerOptionPositionPtr>> all = nullptr;
        auto itr_acc = positions_.find(fund_id);
        if (itr_acc != positions_.end()) {
            auto all = itr_acc->second;
            std::string key = GetKey(code, bs_flag);
            auto itr = all->find(key);
            if (itr != all->end()) {
                pos = itr->second;
            }
        }
        return pos;
    }

    InnerOptionPositionPtr InnerOptionMaster::GetOrCreatePosition(std::string fund_id, std::string code, int64_t bs_flag) {
        InnerOptionPositionPtr pos = nullptr;
        std::shared_ptr<std::map<std::string, InnerOptionPositionPtr>> all = nullptr;
        auto itr_acc = positions_.find(fund_id);
        if (itr_acc != positions_.end()) {
            all = itr_acc->second;
        } else {
            all = std::make_shared<std::map<std::string, InnerOptionPositionPtr>>();
            positions_[fund_id] = all;
        }
        std::string key = GetKey(code, bs_flag);
        auto itr = all->find(key);
        if (itr != all->end()) {
            pos = itr->second;
        } else {
            pos = make_shared<InnerOptionPosition>(fund_id, code, bs_flag);
            (*all)[key] = pos;
        }
        return pos;
    }
}  // namespace co
