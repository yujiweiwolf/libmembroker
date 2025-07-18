// Copyright 2025 Fancapital Inc.  All rights reserved.
#include "inner_option_master.h"

namespace co {
void InnerOptionMaster::InitPositions(MemGetTradePositionMessage* rep) {
    LOG_INFO << "set init option position";
    positions_.clear();
    if (rep->items_size > 0) {
        string fund_id = rep->fund_id;
        auto first = (MemTradePosition*)((char*)rep + sizeof(MemGetTradePositionMessage));
        for (int i = 0; i < rep->items_size; i++) {
            MemTradePosition *position = first + i;
            string code = position->code;
            InnerOptionPositionPtr long_pos = GetPosition(code, kBsFlagBuy);
            InnerOptionPositionPtr short_pos = GetPosition(code, kBsFlagSell);
            long_pos->init_volume_ = position->long_can_close;
            short_pos->init_volume_ = position->short_can_close;
        }
    }
    init_flag_ = true;
    LOG_INFO << "[AutoOpenClose] OnInit";
    for (auto it : positions_) {
        LOG_INFO << it.first << ", long:  " << it.second->first->ToString();
        LOG_INFO << it.first << ", short: " << it.second->second->ToString();
    }
}

void InnerOptionMaster::HandleOrderReq(int64_t bs_flag, const MemTradeOrder& order) {
    // 处理委托请求，冻结数量
    std::string code = order.code;
    int64_t oc_flag = order.oc_flag;
    if (bs_flag != kBsFlagBuy && bs_flag != kBsFlagSell) {
        return;
    }
    if (oc_flag != kOcFlagOpen && oc_flag != kOcFlagClose) {
        return;
    }
    if (!IsAccountInitialized()) {
        return;  // 没有初始化该资金账号的持仓，忽略
    }
    // -------------------------------------------------------------------
    // 获取待更新的持仓
    InnerOptionPositionPtr pos = nullptr;
    if ((bs_flag == kBsFlagBuy && oc_flag == kOcFlagOpen) || (bs_flag == kBsFlagSell && oc_flag == kOcFlagClose)) {
        // 买开和卖平（更新买持仓）
        pos = GetPosition(code, kBsFlagBuy);
    } else if ((bs_flag == kBsFlagSell && oc_flag == kOcFlagOpen) || (bs_flag == kBsFlagBuy && oc_flag == kOcFlagClose)) {
        // 卖开和买平（更新卖持仓）
        pos = GetPosition(code, kBsFlagSell);
    }
    if (pos) {
        std::string before = pos->ToString();
        Update(pos, oc_flag, order.volume, 0, 0);
        std::string after = pos->ToString();
        LOG_INFO << "[AutoOpenClose]["
            << code << ", bs_flag: " << bs_flag << "] OnOrderReq: "
            << "oc_flag: " << oc_flag
            << ", order_volume: " << order.volume
            << ", before " << before << ", after " << after;
    }
}

void InnerOptionMaster::HandleOrderRep(int64_t bs_flag, const MemTradeOrder& order) {
    // 处理委托废单响应，解冻数量
    if (strlen(order.order_no) > 0) {
        return;
    }
    std::string code = order.code;
    int64_t oc_flag = order.oc_flag;
    if (bs_flag != kBsFlagBuy && bs_flag != kBsFlagSell) {
        return;
    }
    if (!IsAccountInitialized()) {
        return;
    }
    // -------------------------------------------------------------------
    // 获取待更新的持仓
    InnerOptionPositionPtr pos = nullptr;
    if ((bs_flag == kBsFlagBuy && oc_flag == kOcFlagOpen) || (bs_flag == kBsFlagSell && oc_flag == kOcFlagClose)) {
        // 买开和卖平（更新买持仓）
        pos = GetPosition(code, kBsFlagBuy);
    } else if ((bs_flag == kBsFlagSell && oc_flag == kOcFlagOpen) || (bs_flag == kBsFlagBuy && oc_flag == kOcFlagClose)) {
        // 卖开和买平（更新卖持仓）
        pos = GetPosition(code, kBsFlagSell);
    }
    if (pos) {
        std::string before = pos->ToString();
        Update(pos, oc_flag, 0, 0, order.volume);
        std::string after = pos->ToString();
        LOG_INFO << "[AutoOpenClose]["
            << code << ", bs_flag: " << bs_flag << "] OnOrderRep: "
            << "oc_flag: " << oc_flag
            << ", withdraw_volume: " << order.volume
            << ", before " << before << ", after " << after;
    }
}

void InnerOptionMaster::HandleKnock(const MemTradeKnock& knock) {
    // 更新内部持仓
    std::string fund_id = knock.fund_id;
    std::string inner_match_no = knock.inner_match_no;
    std::string code = knock.code;
    int64_t bs_flag = knock.bs_flag;
    int64_t oc_flag = knock.oc_flag;
    if (fund_id.empty() || inner_match_no.empty() || code.empty()) {
        return;
    }
    if (bs_flag != kBsFlagBuy && bs_flag != kBsFlagSell) {
        return;
    }
    if (oc_flag != kOcFlagOpen && oc_flag != kOcFlagClose) {
        return;
    }
    if (knock.match_volume <= 0) {
        return;
    }
    if (!IsAccountInitialized()) {
        return;
    }
    auto itr = knocks_.find(inner_match_no);
    if (itr != knocks_.end()) {
        return;  // 该成交回报已处理过，忽略
    }
    knocks_.insert(inner_match_no);
    int64_t match_volume = 0;
    int64_t withdraw_volume = 0;
    if (knock.match_type == kMatchTypeOK) {
        match_volume = knock.match_volume;
    } else if (knock.match_type == kMatchTypeWithdrawOK || knock.match_type == kMatchTypeFailed) {
        withdraw_volume = knock.match_volume;
    }
    InnerOptionPositionPtr pos = nullptr;
    if ((bs_flag == kBsFlagBuy && oc_flag == kOcFlagOpen) || (bs_flag == kBsFlagSell && oc_flag == kOcFlagClose)) {
        // 买开和卖平（更新买持仓）
        pos = GetPosition(code, kBsFlagBuy);
    } else if ((bs_flag == kBsFlagSell && oc_flag == kOcFlagOpen) || (bs_flag == kBsFlagBuy && oc_flag == kOcFlagClose)) {
        // 卖开和买平（更新卖持仓）
        pos = GetPosition(code, kBsFlagSell);
    }
    if (pos && (match_volume > 0 || withdraw_volume > 0)) {
        std::string before = pos->ToString();
        Update(pos, oc_flag, 0, match_volume, withdraw_volume);
        std::string after = pos->ToString();
        LOG_INFO << "[AutoOpenClose]["
            << code << ", bs_flag: " << bs_flag << "] OnKnock: "
            << "oc_flag: " << oc_flag
            << ", match_volume: " << match_volume
            << ", withdraw_volume: " << withdraw_volume
            << ", before " << before << ", after " << after;
    }
}

void InnerOptionMaster::Update(InnerOptionPositionPtr pos, int64_t oc_flag, int64_t order_volume, int64_t match_volume, int64_t withdraw_volume) {
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
            pos->opening_volume_ +=  order_volume;
        }
        if (match_volume > 0) {  // 开仓成交：减少开仓冻结，增加持仓，增加已开仓数
            pos->opening_volume_  -= match_volume;
            pos->open_volume_ += match_volume;
        }
        if (withdraw_volume > 0) {  // 开仓撤单：减少开仓冻结
            if (pos->opening_volume_ > withdraw_volume) {
                pos->opening_volume_ -= withdraw_volume;
            }
        }
        break;
    case kOcFlagClose:  // 平仓
        if (order_volume > 0) {  // 平仓委托：增加平仓冻结
            pos->closing_volume_ += order_volume;
        }
        if (match_volume > 0) {  // 平仓成交：减少平仓冻结，增加已平仓数
            pos->closing_volume_ -= match_volume;
            pos->close_volume_ += match_volume;
        }
        if (withdraw_volume > 0) {  // 平仓撤单：减少平仓冻结
            pos->closing_volume_ -= withdraw_volume;
        }
        break;
    default:
        break;
    }
}

int64_t InnerOptionMaster::GetAutoOcFlag(int64_t bs_flag, const MemTradeOrder& order) {
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
    // int64_t r_bs_flag = ((bs_flag == kBsFlagBuy) ? kBsFlagSell : kBsFlagBuy);
    auto itr_acc = positions_.find(code);
    if (itr_acc == positions_.end()) {  // 没有持仓，直接返回开仓
        return ret_oc_flag;
    }

    InnerOptionPositionPtr pos;
    if (bs_flag == kBsFlagBuy) {
        pos = itr_acc->second->second;  // 买时，先找到对应的空头
    } else {
        pos = itr_acc->second->first;
    }
    if (pos->GetAvailableVolume() >= order_volume) {
        ret_oc_flag = kOcFlagClose;
    }
    return ret_oc_flag;
}

bool InnerOptionMaster::IsAccountInitialized() {
    return init_flag_;
}

InnerOptionPositionPtr InnerOptionMaster::GetPosition(std::string code, int64_t bs_flag) {
    InnerOptionPositionPtr pos = nullptr;
    std::shared_ptr<std::pair<InnerOptionPositionPtr, InnerOptionPositionPtr>> pair = nullptr;
    auto itr_acc = positions_.find(code);
    if (itr_acc != positions_.end()) {
        pair = itr_acc->second;
    } else {
        InnerOptionPositionPtr long_pos = std::make_shared<InnerOptionPosition>(code, kBsFlagBuy);
        InnerOptionPositionPtr short_pos = std::make_shared<InnerOptionPosition>(code, kBsFlagSell);
        pair = std::make_shared<std::pair<InnerOptionPositionPtr, InnerOptionPositionPtr>>();
        pair->first = long_pos;
        pair->second = short_pos;
        positions_[code] = pair;
    }
    if (bs_flag == kBsFlagBuy) {
        pos = pair->first;
    } else if (bs_flag == kBsFlagSell) {
        pos = pair->second;
    }
    return pos;
}
}  // namespace co
