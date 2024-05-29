#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>
#include <queue>
#include <tuple>
#include <thread>
#include <mutex>
#include <atomic>

#include <boost/asio.hpp>

#include "coral/coral.h"
#include "x/x.h"

#include "options.h"
#include "mem_base_broker.h"
#include "mem_processor.h"
#include "upstream.h"

namespace co {
    using namespace std;
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
        bool inited_ = false; // 初始化查询完成，程序启动后要无sleep查询完所有数据；
        bool running_ = false; // 释放正在查询中
        int64_t req_time_ = 0; // 当前请求时间戳
        int64_t rep_time_ = 0; // 当前响应时间戳
        int64_t last_success_time_ = 0; // 最后查询成功的时间戳
        std::string cursor_; // 最后查询游标
        std::string next_cursor_; // 下一次查询游标
    };

    /**
     * 报单服务器
     * @author Guangxu Pan, bajizhh@gmail.com
     * @since 2014-10-27 13:26:28
     * @version 2019-06-05 10:30:12
     */
    class MemBrokerServer {
    public:
        explicit MemBrokerServer();

        void Init(MemBrokerOptionsPtr option, MemBrokerPtr broker);
        void Start();
        void Join();
        void Run();

        bool ExitAccout(const string& fund_id);
        void BeginTask();
        void EndTask();

        void SendQueryTradeAsset(MemGetTradeAssetMessage* req);
        void SendQueryTradePosition(MemGetTradePositionMessage* req);
        void SendQueryTradeKnock(MemGetTradeKnockMessage* req);
        void SendTradeOrder(MemTradeOrderMessage* req);
        void SendTradeWithdraw(MemTradeWithdrawMessage* req);
        void HandInnerCyclicSignal();
        void HandleClearTimeoutMessages();

        // broker中的查询，回写共享内存前，先判断
        bool IsNewMemTradeAsset(MemTradeAsset* asset);
        bool IsNewMemTradePosition(MemTradePosition* pos);
        void UpdataZeroPosition(const string& fund_id);  // 如果仓位变为0，api不返回这条持仓，需要调用此函数
        bool IsNewMemTradeKnock(MemTradeKnock* knock);

        void SendQueryTradeAssetRep(MemGetTradeAssetMessage* rep);
        void SendQueryTradePositionRep(MemGetTradePositionMessage* rep);
        void SendQueryTradeKnockRep(MemGetTradeKnockMessage* rep);
        void SendTradeOrderRep(MemTradeOrderMessage* rep);
        void SendTradeWithdrawRep(MemTradeWithdrawMessage* rep);
        void SendTradeKnock(MemTradeKnock* knock);

        void OnStart();
        
        inline MemBrokerOptionsPtr options() {
            return opt_;
        }

        inline MemBrokerPtr broker() {
            return broker_;
        }

        std::string GetNodeId();
        std::string GetNodeName();

    protected:
        void LoadTradingData();
        void RunQuery();
        void RunWatch();
        void DoWatch();
        void RunStream();
        void RunController();

    private:
        MemBrokerOptionsPtr opt_;
        MemBrokerPtr broker_;
        std::shared_ptr<MemProcessor> processor_;

        boost::asio::io_context ioc_;
        std::vector<std::shared_ptr<Upstream>> upstreams_;

        std::vector<std::shared_ptr<std::thread>> threads_;

        bool enable_upload_asset_ = false;
        bool enable_upload_position_ = false;
        bool enable_upload_knock_ = false;

        std::string node_id_;
        std::string node_name_;
        std::map<std::string, QueryContext*> asset_contexts_;
        std::map<std::string, QueryContext*> position_contexts_;
        std::map<std::string, QueryContext*> knock_contexts_;

        std::map<std::string, MemTradeAsset> assets_;
        std::map<std::string, std::shared_ptr<std::map<std::string, MemTradePosition>>> positions_; // fund_id -> {code -> obj}
        std::set<std::string> knocks_;
        std::set<std::string> pos_code_;

        int64_t active_task_timestamp_ = 0; // 正在执行任务的开始时间
        x::MMapWriter inner_writer_;  // 内部使用
        x::MMapWriter rep_writer_;
        int64_t start_time_ = 0;
        int64_t wait_size_ = 0;  // 查询待处理的消息队列
        int64_t last_heart_beat_ = 0;

        std::unordered_map<std::string, int64_t> pending_orders_;
        std::unordered_map<std::string, int64_t> pending_withdraws_;
    };
}  // namespace co

