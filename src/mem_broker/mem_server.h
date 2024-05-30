// Copyright 2021 Fancapital Inc.  All rights reserved.
#pragma once
#include <vector>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <unordered_map>

#include "x/x.h"
#include "coral/coral.h"
#include "options.h"
#include "mem_base_broker.h"

namespace co {
class QueryContext {
 public:
    inline std::string fund_id() const {
        return fund_id_;
    }
    inline void set_fund_id(std::string fund_id) {
        fund_id_ = fund_id;
    }
    inline std::string fund_name() const {
        return fund_name_;
    }
    inline void set_fund_name(std::string fund_name) {
        fund_name_ = fund_name;
    }
    inline bool inited() const {
        return inited_;
    }
    inline void set_inited(bool inited) {
        inited_ = inited;
    }
    inline int64_t req_time() const {
        return req_time_;
    }
    inline void set_req_time(int64_t ts) {
        req_time_ = ts;
    }
    inline int64_t rep_time() const {
        return rep_time_;
    }
    inline void set_rep_time(int64_t ts) {
        rep_time_ = ts;
    }
    inline int64_t last_success_time() const {
        return last_success_time_;
    }
    inline void set_last_success_time(int64_t ts) {
        last_success_time_ = ts;
    }
    inline std::string cursor() const {
        return cursor_;
    }
    inline void set_cursor(std::string cursor) {
        cursor_ = cursor;
    }
    inline std::string next_cursor() const {
        return next_cursor_;
    }
    inline void set_next_cursor(std::string next_cursor) {
        next_cursor_ = next_cursor;
    }

 private:
    std::string fund_id_;
    std::string fund_name_;
    bool inited_ = false;  // 初始化查询完成，程序启动后要无sleep查询完所有数据；
    bool running_ = false;  // 释放正在查询中
    int64_t req_time_ = 0;  // 当前请求时间戳
    int64_t rep_time_ = 0;  // 当前响应时间戳
    int64_t last_success_time_ = 0;  // 最后查询成功的时间戳
    std::string cursor_;  // 最后查询游标
    std::string next_cursor_;  // 下一次查询游标
};

class MemBrokerServer {
 public:
    MemBrokerServer();

    ~MemBrokerServer();

    void Init(MemBrokerOptionsPtr option, MemBrokerPtr broker);

    void Start();

    void Join();

    void Run();

    bool ExitAccout(const string& fund_id);
    void BeginTask();
    void EndTask();

    void SendRtnMessage(const std::string& raw, int64_t type);

    void OnStart();

 protected:
    void RunQuery();
    void RunWatch();
    void DoWatch();
    void ReadReqMem();
    void HandleQueueMessaage();

    void LoadTradingData();
    bool IsNewMemTradeKnock(MemTradeKnock* knock);
    void CreateInnerMatchNo(MemTradeKnock* knock);

    void SendQueryTradeAsset(MemGetTradeAssetMessage* req);
    void SendQueryTradePosition(MemGetTradePositionMessage* req);
    void SendQueryTradeKnock(MemGetTradeKnockMessage* req);
    void SendTradeOrder(MemTradeOrderMessage* req);
    void SendTradeWithdraw(MemTradeWithdrawMessage* req);

    void SendQueryTradeAssetRep(MemGetTradeAssetMessage* rep);
    void SendQueryTradePositionRep(MemGetTradePositionMessage* rep);
    void SendQueryTradeKnockRep(MemGetTradeKnockMessage* rep);
    void SendTradeOrderRep(MemTradeOrderMessage* rep);
    void SendTradeWithdrawRep(MemTradeWithdrawMessage* rep);
    void SendTradeKnock(MemTradeKnock* knock);

 private:
    MemBrokerOptionsPtr opt_;
    MemBrokerPtr broker_;
    std::string node_name_;
    std::vector<std::shared_ptr<std::thread>> threads_;


    std::map<std::string, QueryContext*> asset_contexts_;
    std::map<std::string, QueryContext*> position_contexts_;
    std::map<std::string, QueryContext*> knock_contexts_;

    std::map<std::string, MemTradeAsset> assets_;
    std::map<std::string, std::shared_ptr<std::map<std::string, MemTradePosition>>> positions_;  // fund_id -> {code -> obj}
    std::set<std::string> knocks_;
    std::set<std::string> pos_code_;

    int64_t active_task_timestamp_ = 0;
    x::MMapWriter rep_writer_;
    int64_t start_time_ = 0;
    int64_t wait_size_ = 0;
    int64_t last_heart_beat_ = 0;

    std::unordered_map<std::string, int64_t> pending_orders_;
    std::unordered_map<std::string, int64_t> pending_withdraws_;
    shared_ptr<StringQueue> queue_;
};
}  // namespace co

