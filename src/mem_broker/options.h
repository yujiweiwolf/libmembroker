#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <memory>

#include "x/x.h"

namespace co {

    /**
     * 柜台接口服务器配置选项
     */
    class MemBrokerOptions {
    public:
        static std::shared_ptr<MemBrokerOptions> Load(const std::string& filename = "");

        std::string ToString();

        inline std::string trade_gateway() const {
            return trade_gateway_;
        }

        inline int64_t request_timeout_ms() const {
            return request_timeout_ms_;
        }

        inline int64_t query_asset_interval_ms() const {
            return query_asset_interval_ms_;
        }

        inline int64_t query_position_interval_ms() const {
            return query_position_interval_ms_;
        }

        inline int64_t query_knock_interval_ms() const {
            return query_knock_interval_ms_;
        }

        inline int64_t idle_sleep_ns() const {
            return idle_sleep_ns_;
        }

        inline int64_t cpu_affinity() const {
            return cpu_affinity_;
        }

        inline bool enable_stock_short_selling() const {
            return enable_stock_short_selling_;
        }

        inline bool enable_query_only() const {
            return enable_query_only_;
        }

        inline std::string wal() const {
            return wal_;
        }

        inline bool enable_upload() const {
            return enable_upload_;
        }
        inline std::string mem_dir() const {
            return mem_dir_;
        }
        inline std::string mem_req_file() const {
            return mem_req_file_;
        }
        inline std::string mem_rep_file() const {
            return mem_rep_file_;
        }


    private:
        std::shared_ptr<x::LoggingOptions> log_opt_;
        std::string trade_gateway_;
        std::string wal_;

        bool enable_upload_ = true; // 是否启用上传交易数据

        int64_t request_timeout_ms_ = 5000; // 请求超时时间

        bool enable_stock_short_selling_ = false; // 启用股票账户融券模式
        bool enable_query_only_ = false; // 是否启用只查询模式，不接收报单和撤单等指令

        int64_t query_asset_interval_ms_ = 0; // 资金查询时间间隔
        int64_t query_position_interval_ms_ = 0; // 持仓查询时间间隔
        int64_t query_knock_interval_ms_ = 0; // 成交查询时间间隔
        int64_t idle_sleep_ns_ = 100000; // 无锁队列空转时休眠时间（单位：纳秒）
        int64_t cpu_affinity_ = -1; // CPU核绑定
        string mem_dir_;
        string mem_req_file_;
        string mem_rep_file_;
    };

    typedef std::shared_ptr<MemBrokerOptions> MemBrokerOptionsPtr;

}
