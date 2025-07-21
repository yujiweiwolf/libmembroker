// Copyright 2025 Fancapital Inc.  All rights reserved.

#include "yaml-cpp/yaml.h"
#include "inner_future_master.h"
constexpr int kCFFEXOptionLength = 14;

namespace co {
void InnerFutureMaster::InitCffexParam() {
    auto getBool = [&](const YAML::Node& node, const std::string& name) {
        try {
            return node[name] && !node[name].IsNull() ? node[name].as<bool>() : false;
        } catch (std::exception& e) {
            LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
            throw std::runtime_error(e.what());
        }
    };
    auto getInt = [&](const YAML::Node& node, const std::string& name, const int64_t& default_value = 0) {
        try {
            return node[name] && !node[name].IsNull() ? node[name].as<int64_t>() : default_value;
        } catch (std::exception& e) {
            LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
            throw std::runtime_error(e.what());
        }
    };
    try {
        auto filename = x::FindFile("broker.yaml");
        YAML::Node root = YAML::LoadFile(filename);
        auto param = root["cffex"];
        forbid_closing_today_ = getBool(param, "forbid_closing_today");
        max_today_opening_volume_ = getInt(param, "max_today_opening_volume");
        LOG_INFO << "股指特定参数 forbid_closing_today: " << forbid_closing_today_ << ", max_today_opening_volume: " << max_today_opening_volume_;
    } catch (std::exception& e) {
        LOG_ERROR << "InitCffexParam, " << e.what();
    }
}

void InnerFutureMaster::InitPositions(MemGetTradePositionMessage* rep) {
    LOG_INFO << "set init future position";
    InitCffexParam();
    positions_.clear();
    open_cache_.insert(std::make_pair("IF", 0));
    open_cache_.insert(std::make_pair("IH", 0));
    open_cache_.insert(std::make_pair("IC", 0));
    open_cache_.insert(std::make_pair("IM", 0));
    if (rep->items_size > 0) {
        string fund_id = rep->fund_id;
        auto first = (MemTradePosition*)((char*)rep + sizeof(MemGetTradePositionMessage));
        for (int i = 0; i < rep->items_size; i++) {
            MemTradePosition *position = first + i;
            string code = position->code;
            InnerFuturePositionPtr long_pos = GetPosition(code, kBsFlagBuy, kOcFlagOpen);
            InnerFuturePositionPtr short_pos = GetPosition(code, kBsFlagSell, kOcFlagOpen);
            long_pos->yd_init_volume_ = position->long_pre_volume;
            long_pos->td_init_volume_ = position->long_volume - position->long_pre_volume;
            short_pos->yd_init_volume_ = position->short_pre_volume;
            short_pos->td_init_volume_ = position->short_volume - position->short_pre_volume;
            if (long_pos->marker_ == kMarketCFFEX && long_pos->td_init_volume_) {
                string type = code.length() > 2 ? code.substr(0, 2) : "";
                if (auto it = open_cache_.find(type); it != open_cache_.end()) {
                    it->second += long_pos->td_init_volume_;
                }
            }
            if (short_pos->marker_ == kMarketCFFEX && short_pos->td_init_volume_) {
                string type = code.length() > 2 ? code.substr(0, 2) : "";
                if (auto it = open_cache_.find(type); it != open_cache_.end()) {
                    it->second += short_pos->td_init_volume_;
                }
            }
        }
    }
    init_flag_ = true;
    LOG_INFO << "[AutoOpenClose] OnInit";
    for (auto it : positions_) {
        LOG_INFO << it.first << ", long:  " << it.second->first->ToString();
        LOG_INFO << it.first << ", short: " << it.second->second->ToString();
    }
}

void InnerFutureMaster::HandleOrderReq(int64_t bs_flag, const MemTradeOrder& order) {
    // 处理委托请求，冻结数量
    std::string code = order.code;
    int64_t market = order.market;
    if (market == 0) {
        market = co::CodeToMarket(code);
    }
    if (bs_flag != kBsFlagBuy && bs_flag != kBsFlagSell) {
        return;
    }
    int64_t oc_flag = order.oc_flag;
    if (oc_flag <= 0) {
        return;
    }
    if (!IsAccountInitialized()) {
        return;
    }

    InnerFuturePositionPtr pos = GetPosition(code, bs_flag, oc_flag);
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

void InnerFutureMaster::HandleOrderRep(int64_t bs_flag, const MemTradeOrder& order) {
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
    InnerFuturePositionPtr pos = GetPosition(code, bs_flag, oc_flag);
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

void InnerFutureMaster::HandleKnock(const MemTradeKnock& knock) {
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
    InnerFuturePositionPtr pos = GetPosition(code, bs_flag, oc_flag);
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

void InnerFutureMaster::Update(InnerFuturePositionPtr pos, int64_t oc_flag, int64_t order_volume, int64_t match_volume, int64_t withdraw_volume) {
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
    int64_t market = pos->marker_;
    switch (oc_flag) {
    case kOcFlagOpen:
        if (order_volume > 0) {  // 开仓委托：增加开仓冻结
            pos->td_opening_volume_ +=  order_volume;
        }
        if (match_volume > 0) {  // 开仓成交：减少开仓冻结，增加持仓，增加已开仓数
            pos->td_opening_volume_  -= match_volume;
            pos->td_open_volume_ += match_volume;
        }
        if (withdraw_volume > 0) {  // 开仓撤单：减少开仓冻结
            if (pos->td_opening_volume_ > withdraw_volume) {
                pos->td_opening_volume_ -= withdraw_volume;
            }
        }
        // 风控策略：更新当前期货类型的已开仓数和开仓冻结数之和
        if (forbid_closing_today_ && pos->marker_ == co::kMarketCFFEX) {
            string code = pos->code_;
            if (code.length() > kCFFEXOptionLength) {
                return;
            }
            string type = code.length() > 2 ? code.substr(0, 2) : "";
            if (auto it = open_cache_.find(type); it != open_cache_.end()) {
                if (order_volume > 0) {
                    it->second += order_volume;
                }
                if (withdraw_volume > 0) {
                    it->second -= order_volume;
                }
            }
        }
        break;
    case kOcFlagClose:  // 平仓
    case kOcFlagForceClose:  // 强平
    case kOcFlagForceOff:  // 强减
    case kOcFlagLocalForceClose:  // 本地强平
        if (order_volume > 0) {
            // SHFE，INE Close相当于平昨指令, CFFEX只能平昨，不能平今
            if (market == co::kMarketSHFE || market == co::kMarketINE || market == co::kMarketCFFEX) {
                pos->yd_closing_volume_ += order_volume;
            } else {  // 先平今，后平昨(先开先平)
                int64_t td_available_pos = pos->GetTodayAvailableVolume();
                if (td_available_pos >= order_volume) {
                    pos->td_closing_volume_ += order_volume;
                } else {
                    pos->td_closing_volume_ += td_available_pos;
                    pos->yd_closing_volume_ += (order_volume - td_available_pos);
                }
            }
        }
        if (match_volume > 0) {
            if (market == co::kMarketSHFE || market == co::kMarketINE || market == co::kMarketCFFEX) {
                if (pos->yd_closing_volume_ >= match_volume) {
                    pos->yd_closing_volume_ -= match_volume;
                }
                pos->yd_close_volume_ += match_volume;
            } else {  // 先平今，后平昨(先开先平)
                int64_t today_pos = 0, yes_today = 0;
                if (pos->td_closing_volume_ >= match_volume) {
                    today_pos = match_volume;
                } else {
                    today_pos = pos->td_closing_volume_;
                    yes_today = match_volume - pos->td_closing_volume_;
                }
                pos->td_closing_volume_ -= today_pos;
                pos->td_close_volume_ += today_pos;
                pos->yd_closing_volume_ -= yes_today;
                pos->yd_close_volume_ += yes_today;
            }
        }
        if (withdraw_volume > 0) {
            if (market == co::kMarketSHFE || market == co::kMarketINE || market == co::kMarketCFFEX) {
                if (pos->yd_closing_volume_ >= withdraw_volume) {
                    pos->yd_closing_volume_ -= withdraw_volume;
                }
            } else {
                int64_t today_pos = 0, yes_today = 0;
                if (pos->td_closing_volume_ >= withdraw_volume) {
                    today_pos = withdraw_volume;
                } else {
                    today_pos = pos->td_closing_volume_;
                    yes_today = withdraw_volume - pos->td_closing_volume_;
                }
                pos->td_closing_volume_ -= today_pos;
                pos->yd_closing_volume_ -= yes_today;
            }
        }
        break;
    case kOcFlagCloseToday:  // 平今
        if (order_volume > 0) {
            pos->td_closing_volume_ += order_volume;
        }
        if (match_volume > 0) {
            if (pos->td_closing_volume_ >= match_volume) {
                pos->td_closing_volume_ -= match_volume;
            }
            pos->td_close_volume_ += match_volume;
        }
        if (withdraw_volume > 0) {
            if (pos->td_closing_volume_ >= withdraw_volume) {
                pos->td_closing_volume_ -= withdraw_volume;
            }
        }
        break;
    case kOcFlagCloseYesterday:  // 平昨
        if (order_volume > 0) {
            pos->yd_closing_volume_ += order_volume;
        }
        if (match_volume > 0) {
            if (pos->yd_closing_volume_ >= match_volume) {
                pos->yd_closing_volume_ -= match_volume;
            }
            pos->yd_close_volume_ += match_volume;
        }
        if (withdraw_volume > 0) {
            if (pos->yd_closing_volume_ >= withdraw_volume) {
                pos->yd_closing_volume_ -= withdraw_volume;
            }
        }
        break;
    default:
        break;
    }
}

int64_t InnerFutureMaster::GetAutoOcFlag(int64_t bs_flag, const MemTradeOrder& order) {
    // 买开（bs_flag=买，oc_flag=自动）:
    // 1.如果有卖方向头寸，则执行：买平；
    // 2.如果没有卖方向头寸或卖方向头寸不足，则执行：买开
    // 卖开（bs_flag=买，oc_flag=自动）：
    // 1.如果有买方向头寸，则执行：卖平;
    // 2.如果没有买方向头寸或买方向头寸不足，则执行：卖开
    if (order.oc_flag != co::kOcFlagAuto) {  // 不是自动开平仓，直接返回请求中设定的开平仓标记
        CheckRisk(order.code, bs_flag, order.oc_flag, order.volume);
        return order.oc_flag;
    }
    int64_t ret_oc_flag = kOcFlagOpen;  // 默认开仓
    string code = order.code;
    int64_t order_volume = order.volume;

    auto itr_acc = positions_.find(code);
    if (itr_acc == positions_.end()) {  // 没有持仓，直接返回开仓
        return ret_oc_flag;
    }

    InnerFuturePositionPtr pos;
    if (bs_flag == kBsFlagBuy) {
        pos = itr_acc->second->second;  // 买时，先找到对应的空头
    } else {
        pos = itr_acc->second->first;   // 卖时，先找到对应的多头
    }

    int64_t market = pos->marker_;
    int64_t yd_available_pos = pos->GetYesterdayAvailableVolume();
    int64_t td_available_pos = pos->GetTodayAvailableVolume();
    if (market == co::kMarketSHFE || market == co::kMarketINE) {
        if (yd_available_pos >= order_volume) {
            ret_oc_flag = kOcFlagCloseYesterday;
        } else if (td_available_pos > order_volume) {
            ret_oc_flag = kOcFlagCloseToday; // 也可以是 kOcFlagClose
        }
    } else if (market == co::kMarketCFFEX) {
        string type = code.length() > 2 ? code.substr(0, 2) : "";
        auto it = open_cache_.find(type);
        if (it->second == 0) {
            if (yd_available_pos >= order_volume) {
                ret_oc_flag = kOcFlagClose;
            }
        } else {
            LOG_INFO << "股指代码: " << order.code << ", 品种: " << type << ", 今仓手数: " << it->second << ", 只能开仓, 不能平仓";
        }
    } else {
        if ((yd_available_pos + td_available_pos) >= order_volume) {
            ret_oc_flag = kOcFlagClose;
        }
    }
    LOG_INFO << "[AutoOpenClose]["
             << code << ", bs_flag: " << bs_flag << "] GetAutoOcFlag: "
             << "oc_flag: " << ret_oc_flag
             << ", order_volume: " << order.volume
             << ", " << pos->ToString();
    return ret_oc_flag;
}

// 平昨仓,不平今仓, 如果昨仓数量不足,就开仓
int64_t InnerFutureMaster::GetCloseYesterdayFlag(int64_t bs_flag, const MemTradeOrder& order) {
    int64_t ret_oc_flag = kOcFlagOpen;  // 默认开仓
    string code = order.code;
    int64_t order_volume = order.volume;

    auto itr_acc = positions_.find(code);
    if (itr_acc == positions_.end()) {  // 没有持仓，直接返回开仓
        return ret_oc_flag;
    }

    InnerFuturePositionPtr pos;
    if (bs_flag == kBsFlagBuy) {
        pos = itr_acc->second->second;  // 买时，先找到对应的空头
    } else {
        pos = itr_acc->second->first;
    }

    int64_t market = pos->marker_;
    int64_t yd_available_pos = pos->yd_init_volume_ + pos->yd_close_volume_ - pos->yd_closing_volume_;
    if (market == co::kMarketSHFE || market == co::kMarketINE) {
        if (yd_available_pos >= order_volume) {
            ret_oc_flag = kOcFlagCloseYesterday;
        }
    } else if (market == co::kMarketCFFEX) {
        string type = code.length() > 2 ? code.substr(0, 2) : "";
        auto it = open_cache_.find(type);
        if (it->second == 0) {
            if (yd_available_pos >= order_volume) {
                ret_oc_flag = kOcFlagClose;
            }
        } else {
            LOG_INFO << "股指代码: " << order.code << " 今天开过仓, 只能开仓, 不能平仓";
        }
    } else {
        if (yd_available_pos >= order_volume) {
            ret_oc_flag = kOcFlagCloseYesterday;
        }
    }
    return ret_oc_flag;
}

void InnerFutureMaster::CheckRisk(string code, int64_t bs_flag, int64_t oc_flag, int64_t order_volume) {
    if (code.length() > kCFFEXOptionLength) {
        return;
    }
    // 风控策略：限制股指期货当日开仓数量
    if (max_today_opening_volume_ >= 0 && oc_flag == kOcFlagOpen) {
        string type = code.length() > 2 ? code.substr(0, 2) : "";
        int64_t open_volume = 0; // 当前期货类型的已开仓数和开仓冻结数之和
        auto it = open_cache_.find(type);
        if (it != open_cache_.end()) {
            open_volume = it->second;
        }
        if (order_volume + open_volume > max_today_opening_volume_) {
            stringstream ss;
            ss << "[开仓数限制]风控检查失败，委托数:" << order_volume << "，已开仓:" << open_volume << "，最大开仓数限制:" << max_today_opening_volume_;
            string str = ss.str();
            throw runtime_error(str);
        }
    }
}

bool InnerFutureMaster::IsAccountInitialized() {
    return init_flag_;
}

// 除了开仓，强平、平今、平昨、强减、本地强平都认为是平仓
InnerFuturePositionPtr InnerFutureMaster::GetPosition(std::string code, int64_t bs_flag, int64_t oc_flag) {
    // 买开和卖平（更新买持仓), 卖开和买平（更新卖持仓
    bool long_flag = false;
    if (bs_flag == kBsFlagBuy) {
        if (oc_flag == kOcFlagOpen) {
            long_flag = true;
        }
    } else {
        if (oc_flag == kOcFlagOpen) {
            long_flag = false;
        } else {
            long_flag = true;
        }
    }
    InnerFuturePositionPtr pos = nullptr;
    std::shared_ptr<std::pair<InnerFuturePositionPtr, InnerFuturePositionPtr>> pair = nullptr;
    auto itr_acc = positions_.find(code);
    if (itr_acc != positions_.end()) {
        pair = itr_acc->second;
    } else {
        InnerFuturePositionPtr long_pos = std::make_shared<InnerFuturePosition>(code, kBsFlagBuy);
        InnerFuturePositionPtr short_pos = std::make_shared<InnerFuturePosition>(code, kBsFlagSell);
        pair = std::make_shared<std::pair<InnerFuturePositionPtr, InnerFuturePositionPtr>>();
        pair->first = long_pos;
        pair->second = short_pos;
        positions_[code] = pair;
    }
    if (long_flag) {
        pos = pair->first;
    } else {
        pos = pair->second;
    }
    return pos;
}
}  // namespace co