#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "x/x.h"
#include "coral/coral.h"
#include "inner_option_master.h"
#include "inner_stock_master.h"
#include "options.h"
#include "mem_struct.h"

using namespace std;
namespace co {

    class  MemBrokerServer;

    class MemBroker {
    public:
        MemBroker();
        virtual ~MemBroker();

        void Init(const MemBrokerOptions& opt, MemBrokerServer* server);

        const std::map<string, MemTradeAccount>& GetAccounts() const;
        co::MemTradeAccount* GetAccount(const string& fund_id);
        bool ExitAccout(const string& fund_id);
        void AddAccount(const co::MemTradeAccount& account);


        inline InnerStockMaster* inner_stock_master() {
            return &inner_stock_master_;
        }

        inline InnerOptionMaster* inner_option_master() {
            return &inner_option_master_;
        }

        void SendQueryTradeAsset(MemGetTradeAssetMessage* req);
        void SendQueryTradePosition(MemGetTradePositionMessage* req);
        void SendQueryTradeKnock(MemGetTradeKnockMessage* req);
        void SendTradeOrder(MemTradeOrderMessage* req);
        void SendTradeWithdraw(MemTradeWithdrawMessage* req);
        void SendHeartBeat();
        void SendTradeKnock(MemTradeKnock* knock);

        void* CreateMemBuffer(int length);
        void PushMemBuffer(int function);

        void SendQueryTradeAssetRep(MemTradeAsset* data);
        void SendQueryTradePositionRep(MemTradePosition* data);
        void SendQueryTradeKnockRep(MemTradeKnock* data);

    protected:
        std::map<string, MemTradeAccount> accounts_;  // 支持的所有资金账号，由底层在on_init()初始化函数中填写， fund_id -> trade_type
        bool enable_stock_short_selling_ = false; // 是否启用股票自动融券买卖
        int64_t request_timeout_ms_ = 0;
        InnerOptionMaster inner_option_master_; // 期权内部持仓管理
        InnerStockMaster inner_stock_master_; // 股票内部持仓管理

        /**
         * 初始化回调函数，由框架调用
         */
        virtual void OnInit();

        /**
         * 启动完成回调函数，由底层调用，调用时机：第一次启动完成或中途断线重连后调用。
         */
        void OnStart();

        virtual void OnQueryTradeAsset(MemGetTradeAssetMessage* req);
        virtual void OnQueryTradePosition(MemGetTradePositionMessage* req);
        virtual void OnQueryTradeKnock(MemGetTradeKnockMessage* req);
        virtual void OnTradeOrder(MemTradeOrderMessage* req);
        virtual void OnTradeWithdraw(MemTradeWithdrawMessage* req);

        int64_t CheckTimeout(int64_t request_time, int64_t ttl_ms);

        x::MMapWriter rep_writer_;
    private:
        MemBroker(const MemBroker&) = delete;
        const MemBroker& operator=(const MemBroker&) = delete;
        MemBrokerServer* server_ = nullptr;
    };
    typedef std::shared_ptr<MemBroker> MemBrokerPtr;
}  // namespace co

