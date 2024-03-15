#include <iostream>
#include <sstream>
#include <string>

#include "test_broker.h"
#include "config.h"

namespace co {

    void TestBroker::OnInit() {
        LOG_INFO << "initialize FakeBroker ...";
        order_no_index_ = x::RawTime();
        batch_no_index_ = x::RawDate();
        auto accounts = Config::Instance()->accounts();
        for (auto& it : accounts) {
            AddAccount(*it.second);
        }
        OnStart();
        rep_thread_ = std::make_shared<std::thread>(std::bind(&TestBroker::HandReqData, this));
        LOG_INFO << "initialize TestBroker successfully";
    }

    void TestBroker::OnQueryTradeAsset(MemGetTradeAssetMessage* req) {
        LOG_INFO << "query asset, fund_id: " << req->fund_id
                 << ", id: " << req->id
                 << ", timestamp: " << req->timestamp;
        int length = sizeof(MemGetTradeAssetMessage);
        void* buffer = new char[length];
        memcpy(buffer, req, length);
        std::unique_lock<std::mutex> lock(mutex_);
        all_req_.emplace(std::pair(req->id, std::pair(kMemTypeQueryTradeAssetReq ,buffer)));
    }

    void TestBroker::OnQueryTradePosition(MemGetTradePositionMessage* req) {
        LOG_INFO << "query position, fund_id: " << req->fund_id
                 << ", id: " << req->id
                 << ", timestamp: " << req->timestamp;
        int length = sizeof(MemGetTradePositionMessage);
        void* buffer = new char[length];
        memcpy(buffer, req, length);
        std::unique_lock<std::mutex> lock(mutex_);
        all_req_.emplace(std::pair(req->id, std::pair(kMemTypeQueryTradePositionReq ,buffer)));
    }

    void TestBroker::OnQueryTradeKnock(MemGetTradeKnockMessage* req) {
        LOG_INFO << "query knock, fund_id: " << req->fund_id
                 << ", id: " << req->id
                 << ", timestamp: " << req->timestamp
                 << ", cursor: " << req->cursor;
        int length = sizeof(MemGetTradeKnockMessage);
        void* buffer = new char[length];
        memcpy(buffer, req, length);
        std::unique_lock<std::mutex> lock(mutex_);
        all_req_.emplace(std::pair(req->id, std::pair(kMemTypeQueryTradeKnockReq ,buffer)));
    }

    void TestBroker::OnTradeOrder(MemTradeOrderMessage* req) {
        auto items = (MemTradeOrder*)((char*)req + sizeof(MemTradeOrderMessage));
        for (int i = 0; i < req->items_size; i++) {
            MemTradeOrder* order = items + i;
            LOG_INFO << "req order, code: " << order->code
                    << ", fund_id: " << req->fund_id
                    << ", timestamp: " << req->timestamp
                    << ", id: " << req->id
                    << ", volume: " << order->volume
                    << ", bs_flag: " << req->bs_flag
                    << ", price: " << order->price;
        }
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * req->items_size;
        void* buffer = new char[length];
        memcpy(buffer, req, length);
        std::unique_lock<std::mutex> lock(mutex_);
        all_req_.emplace(std::pair(req->id, std::pair(kMemTypeTradeOrderReq ,buffer)));
    }

    void TestBroker::OnTradeWithdraw(MemTradeWithdrawMessage* req) {
        LOG_INFO << "req withdraw, fund_id: " << req->fund_id
                        << ", id: " << req->id
                        << ", timestamp: " << req->timestamp
                        << ", order_no: " << req->order_no
                        << ", batch_no: " << req->batch_no;
        int length = sizeof(MemTradeWithdrawMessage);
        void* buffer = new char[length];
        memcpy(buffer, req, length);
        std::unique_lock<std::mutex> lock(mutex_);
        all_req_.emplace(std::pair(req->id, std::pair(kMemTypeTradeWithdrawReq ,buffer)));
    }

    void TestBroker::HandReqData() {
        while (true) {
            x::Sleep(1000);
            std::unique_lock<std::mutex> lock(mutex_);
            for (auto it = all_req_.begin(); it != all_req_.end();) {
                auto& item = it->second;
                switch (item.first) {
                    case kMemTypeTradeOrderReq: {
                        LOG_INFO << "回写报单回报";
                        MemTradeOrderMessage* rep = (MemTradeOrderMessage*)item.second;
                        rep->rep_time = x::RawDateTime();
                        if (rep->items_size > 1) {
                            string batch_no = "batch_no_" + std::to_string(batch_no_index_++);
                            strncpy(rep->batch_no, batch_no.c_str(), batch_no.length());
                        }
                        auto items = (MemTradeOrder*)((char*)rep + sizeof(MemTradeOrderMessage));
                        for (int i = 0; i < rep->items_size; i++) {
                            MemTradeOrder* order = items + i;
                            string order_no = "order_no_" + std::to_string(order_no_index_++);
                            strcpy(order->order_no, order_no.c_str());
                        }
                        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * rep->items_size;
                        void* buffer = rep_writer_.OpenFrame(length);
                        memcpy(buffer, rep, length);
                        rep_writer_.CloseFrame(kMemTypeTradeOrderRep);
                        x::Sleep(100);
                        {
                            for (int i = 0; i < rep->items_size; i++) {
                                MemTradeOrder* order = items + i;
                                int length = sizeof(MemTradeKnock);
                                void* buffer = rep_writer_.OpenFrame(length);
                                MemTradeKnock* knock = (MemTradeKnock*) buffer;
                                knock->timestamp = x::RawDateTime();
                                strcpy(knock->fund_id, rep->fund_id);
                                strcpy(knock->batch_no, rep->batch_no);
                                strcpy(knock->code, order->code);
                                strcpy(knock->order_no, order->order_no);
                                knock->bs_flag = rep->bs_flag;
                                knock->match_volume = order->volume;
                                int index = (order->volume / 100) % 3;
                                if (index == 0) {
                                    knock->match_type = 1;
                                    knock->match_price = order->price;
                                    knock->match_amount = order->price * order->volume * 100;
                                } else if (index == 1) {
                                    knock->match_type = 2;
                                    knock->match_price = 0;
                                    knock->match_amount = 0;
                                } else {
                                    knock->match_type = 3;
                                    knock->match_price = 0;
                                    knock->match_amount = 0;
                                    strcpy(knock->error, "报单价格不正确");
                                }
                                rep_writer_.CloseFrame(kMemTypeTradeKnock);
                            }
                        }
                        break;
                    }
                    case kMemTypeTradeWithdrawReq: {
                        LOG_INFO << "回写撤单回报";
                        int length = sizeof(MemTradeWithdrawMessage);
                        void* buffer = rep_writer_.OpenFrame(length);
                        memcpy(buffer, item.second, length);
                        MemTradeWithdrawMessage* rep = (MemTradeWithdrawMessage*)buffer;
                        rep->rep_time = x::RawDateTime();
                        if (rep->rep_time % 2 == 0) {
                            strcpy(rep->error, "撤单错误，报单已成交");
                        }
                        rep_writer_.CloseFrame(kMemTypeTradeWithdrawRep);
                        break;
                    }
                    case kMemTypeQueryTradeAssetReq: {
                        LOG_INFO << "查询资金响应";
                        MemGetTradeAssetMessage* req = (MemGetTradeAssetMessage*)item.second;
                        int length = sizeof(MemTradeAsset);
                        void* buffer = CreateMemBuffer(length);
                        MemTradeAsset* rep = (MemTradeAsset*)buffer;
                        rep->timestamp = x::RawDateTime();
                        strcpy(rep->fund_id, req->fund_id);
                        rep->balance = 100.0 + req->timestamp % 2;
                        rep->usable = 123.0;
                        PushMemBuffer(kMemTypeQueryTradeAssetRep);
                        break;
                    }
                    case kMemTypeQueryTradePositionReq: {
                        LOG_INFO << "查询持仓响应";
                        int total_num = 5;
                        int length = sizeof(MemGetTradePositionMessage) + sizeof(MemTradePosition) * total_num;
                        MemGetTradePositionMessage* req = (MemGetTradePositionMessage*)item.second;
                        void* buffer = CreateMemBuffer(length);
                        memcpy(buffer, item.second, sizeof(MemGetTradePositionMessage));
                        MemGetTradePositionMessage* rep = (MemGetTradePositionMessage*)buffer;
                        rep->items_size = total_num;
                        MemTradePosition* item = (MemTradePosition*)((char*)buffer + sizeof(MemGetTradePositionMessage));
                        for (int i = 0; i < total_num; i++) {
                            MemTradePosition* pos = item + i;
                            pos->timestamp = x::RawDateTime();
                            strcpy(pos->fund_id, req->fund_id);
                            sprintf(pos->code, "00000%d.SZ", i);
                            pos->market = co::kMarketSZ;
                            pos->long_volume = i * 100 + 10 + req->timestamp % 2;
                        }
                        PushMemBuffer(kMemTypeQueryTradePositionRep);
                        break;
                    }
                    case kMemTypeQueryTradeKnockReq: {
                        LOG_INFO << "查询成交响应";
                        int total_num = 3;
                        int length = sizeof(MemGetTradeKnockMessage) + sizeof(MemTradeKnock) * total_num;
                        MemGetTradeKnockMessage* req = (MemGetTradeKnockMessage*)item.second;
                        void* buffer = CreateMemBuffer(length);
                        memcpy(buffer, item.second, sizeof(MemGetTradeKnockMessage));
                        MemGetTradeKnockMessage* rep = (MemGetTradeKnockMessage*)buffer;
                        rep->items_size = total_num;
                        MemTradeKnock* item = (MemTradeKnock*)((char*)buffer + sizeof(MemGetTradeKnockMessage));
                        for (int i = 0; i < total_num; i++) {
                            MemTradeKnock* knock = item + i;;
                            knock->timestamp = x::RawDateTime();
                            strcpy(knock->fund_id, req->fund_id);
                            strcpy(knock->batch_no, "batch_no_123");
                            sprintf(knock->code, "00000%d.SZ", i);
                            sprintf(knock->order_no, "order_no_%d", i);
                            sprintf(knock->match_no, "match_no_%d", i + req->timestamp % 2 * 100);
                            knock->bs_flag = 1;
                            knock->match_volume = i * 100 + 100;
                            knock->match_type = 1;
                            knock->match_price = 10 + 0.01 * i;
                            knock->match_amount = knock->match_volume * knock->match_volume;
                        }
                        PushMemBuffer(kMemTypeQueryTradeKnockRep);
                        break;
                    }
                }
                delete item.second;
                all_req_.erase(it);
                break;
            }
        }
    }
}  // namespace co

