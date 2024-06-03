// Copyright 2021 Fancapital Inc.  All rights reserved.
#pragma once
#include <utility>
#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>
#include "x/x.h"
#include "coral/coral.h"
#include "../mem_broker/mem_base_broker.h"
#include "../mem_broker/mem_server.h"

using namespace std;
namespace co {
class TestBroker: public MemBroker {
 public:
    TestBroker() = default;
    ~TestBroker() = default;

 protected:
    void OnInit();
    void OnQueryTradeAsset(MemGetTradeAssetMessage* req);
    void OnQueryTradePosition(MemGetTradePositionMessage* req);
    void OnQueryTradeKnock(MemGetTradeKnockMessage* req);
    void OnTradeOrder(MemTradeOrderMessage* req);
    void OnTradeWithdraw(MemTradeWithdrawMessage* req);

 private:
    void HandReqData();

 private:
    string spot_fund_id_;
    string future_fund_id_;
    string option_fund_id_;
    int order_no_index_ = 0;
    int batch_no_index_ = 0;

    std::shared_ptr<std::thread> rep_thread_ = nullptr;
    std::mutex mutex_;
    std::unordered_map<std::string, std::pair<int64_t, void*>> all_req_;
};
}  // namespace co

