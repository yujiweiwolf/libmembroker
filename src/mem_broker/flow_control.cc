// Copyright 2025 Fancapital Inc.  All rights reserved.
#include "flow_control.h"

namespace co {
FlowControlStateHolder::FlowControlStateHolder(std::shared_ptr<x::MMapFrame> frame): frame_(std::move(frame)) {
    state_ = (MemFlowControlState*)frame_->mutable_data();
}

FlowControlItem::FlowControlItem(int64_t timestamp, int64_t priority, int64_t cmd_size, double order_amount, double total_amount, int64_t timeout, BrokerMsg* msg):
    timestamp_(timestamp), priority_(priority), cmd_size_(cmd_size), order_amount_(order_amount), total_amount_(total_amount), timeout_(timeout), msg_(msg) {
}

void FlowControlMarketQueue::InitState(std::shared_ptr<FlowControlStateHolder> state_holder) {
    state_holder_ = state_holder;
    total_cmd_size_ = state_holder_->state()->total_cmd_size;
}

BrokerMsg* FlowControlMarketQueue::TryPop(int64_t now_dt) {
    BrokerMsg* ret = TryPopPrimary(now_dt);
    if (!ret) {
        ret = TryPopSecondary(now_dt);
    }
    return ret;
}

BrokerMsg* FlowControlMarketQueue::TryPopPrimary(int64_t now_dt) {
    // 弹出最高优先级的请求（报撤单请求）
    BrokerMsg* ret = nullptr;
    if (now_dt <= 0) {
        now_dt = x::RawDateTime();
    }
    Sort();
    // 第1优先级】优先进行报撤单
    if (!flow_control_queue_.empty()) {
        // 目前交易所的流控要求是：每秒钟不能超过600笔交易指令，否则就被认定为高频交易。
        // 即使broker精确按照流速发送指令，如果考虑网络延迟或者柜台端的卡顿现象，指令到达交易所时仍然可能存在超过流控阈值的情况；
        // 所以这里必须加上一定的安全垫，防止发送出去的指令间隔被压缩，导致流速超出阈值的现象。
        while (!sent_ns_queue_.empty()) {
            auto& sent_dt = sent_ns_queue_.front();
            if (x::SubRawDateTime(now_dt, sent_dt) > kFlowControlWindowMS) {  // 弹出大于1秒的发送时间，给500ms的安全垫；
                sent_ns_queue_.pop_front();
            } else {
                break;
            }
        }
        if (!flow_control_queue_.empty()) {
            auto& item = flow_control_queue_.front();
            int64_t sub_size = item->cmd_size();
            int64_t tps = (int64_t)sent_ns_queue_.size() + sub_size;
            // -----------------------------------------------------
            bool is_timeout = IsTimeout(now_dt, *item, 0);
            if (is_timeout) {  // 在BaseBroker中会进行超时判断，这里可以不进行超时判断，为了单元测试暂时保留；
                ret = CreateTimeoutRep(now_dt, item.get(), 0);
                flow_control_queue_.pop_front();
                cmd_size_ -= sub_size;
            } else if (th_daily_limit_ > 0 && total_cmd_size_ + sub_size > th_daily_limit_) {
                std::stringstream ss;
                ss << "[FAN-Broker-FlowControlError] commands exceed th_daily_limit("
                    << th_daily_limit_ << "), market: " << market_ << ", req_cmd_size: " << sub_size
                    << ", total_cmd_size: " << total_cmd_size_;
                std::string error = ss.str();
                ret = CreateErrorRep(item->msg(), error);
                flow_control_queue_.pop_front();
                cmd_size_ -= sub_size;
                // 只要出现新的请求被打回，就需要持续播放警告，以防交易员漏听。通过设置pre_warning_total_cmd_size_为零来实现；
                pre_warning_total_cmd_size_ = 0;
            } else {
                if (th_tps_limit_ <= 0 || tps <= th_tps_limit_) {
                    ret = item->msg();
                    for (int i = 0; i < sub_size; ++i) {
                        sent_ns_queue_.emplace_back(now_dt);
                    }
                    total_cmd_size_ += sub_size;
                    if (state_holder_) {  // 将状态更新到共享内存，异步持久化到磁盘中；
                        state_holder_->state()->total_cmd_size = total_cmd_size_;
                    }
                    flow_control_queue_.pop_front();
                    cmd_size_ -= sub_size;
                } else {  // 触发流控，需要暂缓报撤单；
                    triggered_flow_control_size_ = cmd_size_;
                }
            }
        }
    }
    // 【第2优先级】如果不需要报单，则尝试处理其他消息
    // 【第3优先级】处理等待超时的报单，撤单无超时，检查队列最后一个即可；
    return ret;
}

BrokerMsg* FlowControlMarketQueue::TryPopSecondary(int64_t now_dt) {
    // 弹出次优先级的消息
    BrokerMsg* ret = nullptr;
    if (now_dt <= 0) {
        now_dt = x::RawDateTime();
    }
    Sort();
    // 【第1优先级】优先进行报撤单
    // 【第2优先级】如果不需要报单，则尝试处理其他消息
    if (!ret && !normal_queue_.empty()) {
        ret = normal_queue_.front();
        normal_queue_.pop_front();
    }
    // 【第3优先级】处理等待超时的报单，撤单无超时，检查队列最后一个即可；
    if (!ret && !flow_control_queue_.empty()) {
        auto& item = flow_control_queue_.back();
        int64_t sub_size = item->cmd_size();
        int64_t ahead_count = cmd_size_ - sub_size;
        bool is_timeout = IsTimeout(now_dt, *item, ahead_count);
        if (is_timeout) {
            ret = CreateTimeoutRep(now_dt, item.get(), ahead_count);
            flow_control_queue_.pop_back();
            cmd_size_ -= sub_size;
        }
    }
    return ret;
}

void FlowControlMarketQueue::Push(std::unique_ptr<FlowControlItem> item) {
    if (th_tps_limit_ > 0 && item->cmd_size() > th_tps_limit_) {
        // 批量委托大小超过了流控阈值
        std::stringstream ss;
        ss << "[FAN-Broker-FlowControlError] ";
        if (item->msg()->function_id() == co::kFuncFBTradeOrderReq) {
            ss << "batch order size exceed flow control threshold, orders: ";
        } else if (item->msg()->function_id() == co::kFuncFBTradeWithdrawReq) {
            ss << "batch withdraw size exceed flow control threshold, orders: ";
        } else {
            ss << "request command size exceed flow control threshold: ";
        }
        ss << item->cmd_size() << ", th_tps_limit: " << th_tps_limit_;
        std::string error = ss.str();
        auto rep_msg = CreateErrorRep(item->msg(), error);
        normal_queue_.emplace_back(rep_msg);
        return;
    }
    cmd_size_ += item->cmd_size();
    flow_control_queue_.emplace_back(std::move(item));
    need_sort_ = true;
}

void FlowControlMarketQueue::Sort() {
    if (need_sort_) {
        need_sort_ = false;
//            std::stable_sort(flow_control_queue_.begin(), flow_control_queue_.end(), [](auto& lhs, auto& rhs) {
//                // 排序优先级：撤单 > 申赎 > 其他，其他类型按委托金额从大到小排序；
//                // 对于撤单，并没有按子委托数量进行排序，因为批量撤单只在手工界面使用，robot发送的全部是单笔撤单，按时间先后顺序排序问题不大；
//                return lhs->priority() != rhs->priority() ? lhs->priority() > rhs->priority() : lhs->order_amount() > rhs->order_amount();
//            });
        std::stable_sort(flow_control_queue_.begin(), flow_control_queue_.end(), [](auto& lhs, auto& rhs) {
            return lhs->total_amount() > rhs->total_amount();
        });
    }
}

void FlowControlMarketQueue::PopWarningMessage(const std::string& node_name, std::string* text) {
    if (th_daily_warning_ > 0 && total_cmd_size_ >= th_daily_warning_ && pre_warning_total_cmd_size_ != total_cmd_size_) {
        pre_warning_total_cmd_size_ = total_cmd_size_;
        std::stringstream ss;
        ss << "【" << node_name << "】[" << co::MarketToText(market_) << "]全天报撤单笔数已达到：" << total_cmd_size_ << "笔";
        if (th_daily_limit_ > 0 && total_cmd_size_ >= th_daily_limit_) {
            ss << "，已禁止交易";
        }
        (*text) = ss.str();
    } else if (triggered_flow_control_size_ > 0) {
        std::stringstream ss;
        ss << "【" << node_name << "】[" << co::MarketToText(market_) << "]触发交易流控，排队长度：" << triggered_flow_control_size_;
        (*text) = ss.str();
        triggered_flow_control_size_ = 0;
    }
}

bool FlowControlMarketQueue::IsTimeout(int64_t now_dt, const FlowControlItem& item, int64_t ahead_count) const {
    // 检查请求时间是否已超时，如果流控队列前面排队太长，也可能会导致当前请求立即超时；
    bool timeout = false;
    int64_t timeout_ms = item.timeout();
    if (timeout_ms > 0 && item.priority() != kFlowControlPriorityWithdraw) {
        int64_t req_delay_ms = x::SubRawDateTime(now_dt, item.timestamp());
        int64_t fc_delay_ms = th_tps_limit_ > 0 && ahead_count > 0 ? req_delay_ms + (ahead_count / th_tps_limit_) * 1000 : 0;
        timeout = std::abs(req_delay_ms) >= timeout_ms || std::abs(fc_delay_ms) >= timeout_ms;
    }
    return timeout;
}

BrokerMsg* FlowControlMarketQueue::CreateTimeoutRep(int64_t now_dt, FlowControlItem* item, int64_t ahead_count) {
    int64_t req_delay_ms = x::SubRawDateTime(now_dt, item->timestamp());
    int64_t fc_ahead_ms = th_tps_limit_ > 0 && ahead_count > 0 ? (ahead_count / th_tps_limit_) * 1000 : 0;
    std::stringstream ss;
    ss << "[FAN-Broker-TimeoutError] flow control timeout for " << req_delay_ms << "ms, fc_queue: "
       << flow_control_queue_.size() << ", fc_sub_size: " << cmd_size_
       << ", fc_ahead: " << fc_ahead_ms << "ms, now: " << now_dt
       << ", timestamp: " << item->timestamp() << ", limit: " << request_timeout_ms_ << "ms";
    std::string error = ss.str();
    return CreateErrorRep(item->msg(), error);
}

BrokerMsg* FlowControlMarketQueue::CreateErrorRep(BrokerMsg* msg, const std::string& error) {
    int64_t req_function_id = msg->function_id();
    if (req_function_id == kMemTypeTradeOrderReq) {
        auto& raw = msg->data();
        int64_t length = raw.length();
        MemTradeOrderMessage *rep = (MemTradeOrderMessage*)raw.data();
        strncpy(rep->error, error.c_str(), error.length());
        msg->set_function_id(kMemTypeTradeOrderRep);
        msg->set_data(string(reinterpret_cast<const char*>(rep), length));
    } else if (req_function_id == kMemTypeTradeWithdrawReq) {
        auto& raw = msg->data();
        MemTradeWithdrawMessage *rep = (MemTradeWithdrawMessage*)raw.data();
        strncpy(rep->error, error.c_str(), error.length());
        msg->set_function_id(kMemTypeTradeWithdrawRep);
        msg->set_data(string(reinterpret_cast<const char*>(rep), sizeof(MemTradeWithdrawMessage)));
    } else {
        throw std::runtime_error("[FAN-Broker-NeverHappenError]");
    }
    return msg;
}


// ============================================================================================

FlowControlQueue::FlowControlQueue(BrokerQueue* broker_queue): broker_queue_(broker_queue) {
}

void FlowControlQueue::Init(MemBrokerOptionsPtr opt) {
    for (auto& cfg : opt->flow_controls()) {
        auto market = cfg->market();
        auto queue = std::make_unique<FlowControlMarketQueue>();
        queue->set_market(market);
        queue->set_request_timeout_ms(request_timeout_ms_);
        queue->set_th_tps_limit(cfg->th_tps_limit());
        queue->set_th_daily_warning(cfg->th_daily_warning());
        queue->set_th_daily_limit(cfg->th_daily_limit());
        market_to_queue_[queue->market()] = queue.get();
        fc_queues_.emplace_back(std::move(queue));
    }
}

void FlowControlQueue::InitState(const std::string& fund_id) {
    // 初始化状态，在server初始化时会调用，单元测试一般不调用；
    LOG_INFO << "[FlowControl] init state, fund_id: " << fund_id << " ...";
    int64_t now_dt = x::RawDateTime();
    int64_t now_date = now_dt / 1000000000LL;
    // -----------------------------------------------------
    // 加载历史状态
    std::unordered_map<std::string, std::unique_ptr<MemFlowControlState>> history_states;
    x::MMapReader reader;
    reader.Open(state_path_, "meta");
    reader.SeekToEnd();
    while (true) {
        const void *data = nullptr;
        int64_t type = reader.Prev(&data);
        if (type == 0) {
            break;
        }
        if (type == kMemTypeFlowControlState) {
            auto q = (MemFlowControlState *) data;
            int64_t state_date = q->timestamp / 1000000000LL;
            if (state_date == now_date && q->total_cmd_size > 0) {
                std::string key = std::string(q->fund_id) + "\t" + std::to_string(q->market);
                auto itr = history_states.find(key);
                if (itr == history_states.end()) {
                    auto state = std::make_unique<MemFlowControlState>();
                    memcpy(state.get(), q, sizeof(*q));
                    history_states[key] = std::move(state);
                }
            } else if (state_date < now_date) {
                break;
            }
        }
    }
    // --------------------------------------------------------------
    meta_writer_.Open(state_path_, "meta", 64 << 20);
    for (auto &queue : fc_queues_) {
        int64_t history_total_cmd_size = 0;
        std::string key = fund_id + "\t" + std::to_string(queue->market());
        auto itr = history_states.find(key);
        if (itr != history_states.end()) {
            auto &state = itr->second;
            history_total_cmd_size = state->total_cmd_size;
            LOG_INFO << "[FlowControl] load history state ok, fund_id: " << state->fund_id
                     << ", market: " << state->market << ", total_cmd_size: " << state->total_cmd_size;
        }
        auto frame = meta_writer_.OpenSharedFrame(sizeof(MemFlowControlState));
        auto state = (MemFlowControlState *) frame->mutable_data();
        memset(state, 0, sizeof(*state));
        state->timestamp = now_dt;
        if (fund_id.size() >= sizeof(state->fund_id)) {
            std::stringstream ss;
            ss << "fund_id exceed length limit(" << sizeof(state->fund_id) << "): " << fund_id.size();
            throw std::runtime_error(ss.str());
        }
        strncpy(state->fund_id, fund_id.c_str(), sizeof(state->fund_id) - 1);
        state->market = queue->market();
        state->total_cmd_size = history_total_cmd_size;
        meta_writer_.CloseFrame(kMemTypeFlowControlState);
        auto state_holder = std::make_shared<FlowControlStateHolder>(frame);
        queue->InitState(state_holder);
    }
    LOG_INFO << "[FlowControl] init state ok";
}

BrokerMsg* FlowControlQueue::Pop() {
    BrokerMsg* ret= nullptr;
    while (!ret) {
        ret = TryPop();
        if (!ret && idle_sleep_ns_ > 0) {
            x::NanoSleep(idle_sleep_ns_);
        }
    }
    return ret;
}

BrokerMsg* FlowControlQueue::TryPop(int64_t now_dt) {
    BrokerMsg* ret = nullptr;
    if (now_dt <= 0) {
        now_dt = x::RawDateTime();
    }
    // 预处理：将底层队列中的消息全部取出来，放入对应的流控队列中，目的是尽快处理掉高优先级的消息
    if (!broker_queue_->Empty()) {
        while (true) {
            auto msg = broker_queue_->TryPop();
            if (!msg) {
                break;
            }
            Push(msg);
        }
    }
    for (auto& queue: fc_queues_) {
        ret = queue->TryPopPrimary(now_dt);
        if (ret) {
            break;
        }
        ret = queue->TryPopSecondary(now_dt);
        if (ret) {
            break;
        }
    }
    // 尝试处理其他消息
    if (!ret && !normal_queue_.empty()) {
        ret = normal_queue_.front();
        normal_queue_.pop_front();
    }
    return ret;
}

void FlowControlQueue::Push(BrokerMsg* msg) {
    int64_t function_id = msg->function_id();
    if (function_id == kMemTypeTradeOrderReq) {
        string raw = msg->data();
        MemTradeOrderMessage *req = reinterpret_cast<MemTradeOrderMessage*>(raw.data());
        int64_t items_size = req->items_size;
        int64_t market = 0;
        MemTradeOrder* items = req->items;
        if (items_size > 0) {
            MemTradeOrder* first = items;
            market = first->market;
            if (market <= 0) {
                market = co::CodeToMarket(first->code ? first->code : "");
            }
        }
        auto itr = market_to_queue_.find(market);
        if (itr != market_to_queue_.end()) {
            auto& queue = itr->second;
            int64_t timestamp = req->timestamp;
            int64_t priority_type = 0;
            double order_amount = 0;
            int64_t bs_flag = req->bs_flag;
            if (bs_flag == kBsFlagCreate || bs_flag == kBsFlagRedeem) {
                priority_type = kFlowControlPriorityCreateRedeem;
            } else {
                priority_type = kFlowControlPriorityOthers;
                for (int64_t i = 0; i < items_size; ++i) {
                    auto item = items + i;
                    // 不考虑期货期权品种，期货期权broker暂不会启用流控功能；
                    order_amount += item->price * (double)item->volume;
                }
            }
            double total_amount = priority_type * kItemMultiple + order_amount;
            int64_t timeout = req->timeout > 0 && req->timeout < request_timeout_ms_ ? req->timeout : request_timeout_ms_;
            auto item = std::make_unique<FlowControlItem>(timestamp, priority_type, items_size, order_amount, total_amount, timeout, msg);
            queue->Push(std::move(item));
        } else {
            bool is_required = FlowControlQueue::IsFlowControlRequiredMarket(market);
            if (is_required) {
                std::string error = "[FAN-Broker-FlowControlError] no flow control config for market: " + std::to_string(market);
                msg = FlowControlMarketQueue::CreateErrorRep(msg, error);
                normal_queue_.emplace_back(msg);
            } else {
                normal_queue_.emplace_back(msg);
            }
        }
    } else if (function_id == kMemTypeTradeWithdrawReq) {
        auto raw = msg->data();
        MemTradeWithdrawMessage *req = reinterpret_cast<MemTradeWithdrawMessage*>(raw.data());
        int64_t market = 0;
        int64_t batch_size = 0;
        if (req->order_no[0] != '\0') {
            ParseStandardOrderNo(req->order_no, &market);
            batch_size = 1;
        } else if (req->batch_no[0] != '\0') {
            ParseStandardBatchNo(req->batch_no, &market, &batch_size);
        }
        if (market <= 0) {  // 解析委托合同号/批次号失败
            std::stringstream ss;
            ss << "[FAN-Broker-FlowControlError] non-standard ";
            if (req->order_no[0] != '\0') {
                ss << "order_no is forbidden: " << req->order_no;
            } else {
                ss << "batch_no is forbidden: " << req->batch_no;
            }
            std::string error = ss.str();
            msg = FlowControlMarketQueue::CreateErrorRep(msg, error);
            normal_queue_.emplace_back(msg);
        } else {
            auto itr = market_to_queue_.find(market);
            if (itr != market_to_queue_.end()) {
                auto& queue = itr->second;
                int64_t timestamp = 0;
                int64_t priority_type = kFlowControlPriorityWithdraw;
                double order_amount = 0;
                double total_amount = priority_type * kItemMultiple + order_amount;
                auto item = std::make_unique<FlowControlItem>(timestamp, priority_type, batch_size, order_amount, total_amount, 0, msg);
                queue->Push(std::move(item));
            } else {
                bool is_required = FlowControlQueue::IsFlowControlRequiredMarket(market);
                if (is_required) {
                    std::string error = "[FAN-Broker-FlowControlError] no flow control config for market: " + std::to_string(market);
                    msg = FlowControlMarketQueue::CreateErrorRep(msg, error);
                    normal_queue_.emplace_back(msg);
                } else {
                    normal_queue_.emplace_back(msg);
                }
            }
        }
    } else {
        normal_queue_.emplace_back(msg);
    }
}

int64_t FlowControlQueue::GetNormalQueueSize() const {
    return (int64_t)normal_queue_.size();
}

void FlowControlQueue::GetFlowControlQueueSize(int64_t* cmd_size, int64_t* total_cmd_size) const {
    (*cmd_size) = 0;
    (*total_cmd_size) = 0;
    for (auto& queue : fc_queues_) {
        (*cmd_size) += queue->cmd_size();
        (*total_cmd_size) += queue->total_cmd_size();
    }
}

std::string FlowControlQueue::PopWarningMessage(const std::string& node_name) {
    std::string text;
    for (auto& queue : fc_queues_) {
        queue->PopWarningMessage(node_name, &text);
        if (!text.empty()) {
            break;
        }
    }
    return text;
}

bool FlowControlQueue::IsFlowControlRequiredMarket(int64_t market) {
    return market == kMarketSH || market == kMarketSZ || market == kMarketBJ;
}
}  // namespace co