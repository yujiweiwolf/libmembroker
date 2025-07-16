// Copyright 2025 Fancapital Inc.  All rights reserved.
#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <memory>

#include "x/x.h"
#include "coral/coral.h"
#include "options.h"
#include "queue.h"

namespace co {
constexpr int64_t kFlowControlPriorityWithdraw = 3;
constexpr int64_t kFlowControlPriorityCreateRedeem = 2;
constexpr int64_t kFlowControlPriorityOthers = 1;
constexpr int64_t kItemMultiple = 100000000;

constexpr int64_t kFlowControlWindowMS = 1500;  // 流控时间窗口，1秒+500ms安全垫；

constexpr int64_t kMemTypeFlowControlState = 1390001;
#ifdef _WIN32
#pragma pack(push, 1)
#endif
struct MemFlowControlState {
    int64_t timestamp;
    char fund_id[co::kMemFundIdSize];
    int64_t market;
    int64_t total_cmd_size;
}
#ifndef _WIN32
        __attribute__((packed));
#else
};
#pragma pack(pop)
#endif

class FlowControlStateHolder {
 public:
    explicit FlowControlStateHolder(std::shared_ptr<x::MMapFrame> frame);

    inline MemFlowControlState* state() {
        return state_;
    }

 private:
    MemFlowControlState* state_ = nullptr;
    std::shared_ptr<x::MMapFrame> frame_ = nullptr;
};

class FlowControlItem {
 public:
    FlowControlItem(int64_t timestamp, int64_t priority, int64_t cmd_size, double order_amount, double total_amount, int64_t timeout, BrokerMsg* msg);

    [[nodiscard]] inline int64_t timestamp() const {
        return timestamp_;
    }

    [[nodiscard]] inline int64_t priority() const {
        return priority_;
    }

    [[nodiscard]] inline int64_t cmd_size() const {
        return cmd_size_;
    }

    [[nodiscard]] inline double order_amount() const {
        return order_amount_;
    }

    [[nodiscard]] inline double total_amount() const {
        return total_amount_;
    }

    [[nodiscard]] inline int64_t timeout() const {
        return timeout_;
    }

    inline BrokerMsg* msg() {
        return msg_;
    }

 private:
    int64_t timestamp_ = 0;  // 消息时间
    int64_t priority_ = 0;  // 类型，3-撤单，2-申赎，1-其他
    int64_t cmd_size_ = 1;  // 流控个数，委托个数或撤单个数；
    double order_amount_ = 0;  // 其他类型的委托金额
    double total_amount_ = 0;  // priority * 100000000 + order_amount_
    int64_t timeout_ = 0;  // 超时毫秒数，取min(req.timeout, cfg.timeout)
    BrokerMsg* msg_ = nullptr;  // BrokerQueue中的元素
};

/**
 * 根据市场进行分组的流控队列
 */
class FlowControlMarketQueue {
 public:
    void InitState(std::shared_ptr<FlowControlStateHolder> state_holder);

    BrokerMsg* TryPop(int64_t now_dt = 0);
    BrokerMsg* TryPopPrimary(int64_t now_dt = 0);
    BrokerMsg* TryPopSecondary(int64_t now_dt = 0);

    void Push(std::unique_ptr<FlowControlItem> item);
    void PopWarningMessage(const std::string& node_name, std::string* text);
    static BrokerMsg* CreateErrorRep(BrokerMsg* msg, const std::string& error);

    inline int64_t market() const {
        return market_;
    }

    inline void set_market(int64_t market) {
        market_ = market;
    }

    inline void set_request_timeout_ms(int64_t request_timeout_ms) {
        request_timeout_ms_ = request_timeout_ms;
    }

    inline int64_t request_timeout_ms() const {
        return request_timeout_ms_;
    }

    inline void set_th_tps_limit(int64_t th_tps_limit) {
        th_tps_limit_ = th_tps_limit;
    }

    inline int64_t th_tps_limit() const {
        return th_tps_limit_;
    }

    inline void set_th_daily_warning(int64_t th_daily_warning) {
        th_daily_warning_ = th_daily_warning;
    }

    inline int64_t th_daily_warning() const {
        return th_daily_warning_;
    }

    inline void set_th_daily_limit(int64_t th_daily_limit) {
        th_daily_limit_ = th_daily_limit;
    }

    inline int64_t th_daily_limit() const {
        return th_daily_limit_;
    }

    inline int64_t cmd_size() const {
        return cmd_size_;
    }

    inline int64_t total_cmd_size() const {
        return total_cmd_size_;
    }

    inline int64_t triggered_flow_control_size() const {
        return triggered_flow_control_size_;
    }

    inline void clear_triggered_flow_control_size() {
        triggered_flow_control_size_ = 0;
    }

 protected:
    void Sort();
    bool IsTimeout(int64_t now_dt, const FlowControlItem& item, int64_t ahead_count) const;
    BrokerMsg* CreateTimeoutRep(int64_t now_dt, FlowControlItem* item, int64_t ahead_count);

 private:
    int64_t market_ = 0;  // 市场代码
    int64_t th_tps_limit_ = 0;  // 每秒报撤单流控阈值
    int64_t th_daily_warning_ = 0;  // 全天报撤单预警阈值
    int64_t th_daily_limit_ = 0;  // 全天报撤单流控阈值
    int64_t request_timeout_ms_ = 0;  // 报单超时阈值

    std::shared_ptr<FlowControlStateHolder> state_holder_ = nullptr;

    std::deque<int64_t> sent_ns_queue_;  // 系统发送时间队列
    bool need_sort_ = false;
    std::deque<std::unique_ptr<FlowControlItem>> flow_control_queue_;  // 需要进行流控的消息队列
    std::deque<BrokerMsg*> normal_queue_;  // 不需要进行流控的其他消息队列

    int64_t cmd_size_ = 0;  // 当前流控队列中的子指令数量之和；
    int64_t total_cmd_size_ = 0;  // 已发送的所有指令个数

    std::atomic_int64_t triggered_flow_control_size_ = 0;  // 已触发流控的最新流控队列大小，用于报警；
    std::atomic_int64_t pre_warning_total_cmd_size_ = 0;  // 上次报警的总指令个数
};

/**
 * FlowControlQueue 流控队列
 * @since 2024-07-29 16:00:32
 */
class FlowControlQueue {
 public:
    explicit FlowControlQueue(BrokerQueue* broker_queue);
    static bool IsFlowControlRequiredMarket(int64_t market);

    void Init(MemBrokerOptionsPtr opt);
    void InitState(const std::string& fund_id);
    BrokerMsg* Pop();
    BrokerMsg* TryPop(int64_t now_dt = 0);

    [[nodiscard]] int64_t GetNormalQueueSize() const;
    void GetFlowControlQueueSize(int64_t* cmd_size, int64_t* total_cmd_size) const;
    std::string PopWarningMessage(const std::string& node_name);

    inline void set_request_timeout_ms(int64_t request_timeout_ms) {
        request_timeout_ms_ = request_timeout_ms;
    }

    [[nodiscard]] inline int64_t request_timeout_ms() const {
        return request_timeout_ms_;
    }

    inline void set_idle_sleep_ns(int64_t idle_sleep_ns) {
        idle_sleep_ns_ = idle_sleep_ns;
    }

    [[nodiscard]] inline int64_t idle_sleep_ns() const {
        return idle_sleep_ns_;
    }

    [[nodiscard]] inline const std::string& state_path() const {
        return state_path_;
    }

 protected:
    void Push(BrokerMsg* msg);

 private:
    flatbuffers::FlatBufferBuilder fbb_;
    int64_t request_timeout_ms_ = 0; // 报单超时阈值
    int64_t idle_sleep_ns_ = 0;
    BrokerQueue* broker_queue_ = nullptr;

    std::vector<std::unique_ptr<FlowControlMarketQueue>> fc_queues_;  // 按市场分组的流控队列
    std::unordered_map<int64_t, FlowControlMarketQueue*> market_to_queue_;
    std::deque<BrokerMsg*> normal_queue_;  // 不需要进行流控的其他消息队列

    std::string state_path_ = "../data/state.broker.mem";  // 状态持久化路径
    x::MMapWriter meta_writer_;
};
}  // namespace co
