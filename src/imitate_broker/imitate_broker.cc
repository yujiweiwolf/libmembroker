// Copyright 2021 Fancapital Inc.  All rights reserved.
#include "imitate_broker.h"
#include "config.h"

namespace co {

    void TestBroker::OnInit() {
        LOG_INFO << "initialize FakeBroker ...";
        srand(time(0));
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
        all_req_.emplace(std::pair(req->id,  std::pair(kMemTypeQueryTradeAssetReq,  buffer)));
    }

    void TestBroker::OnQueryTradePosition(MemGetTradePositionMessage* req) {
        LOG_INFO << "query position, fund_id: " << req->fund_id
                 << ", id: " << req->id
                 << ", timestamp: " << req->timestamp;
        int length = sizeof(MemGetTradePositionMessage);
        void* buffer = new char[length];
        memcpy(buffer, req, length);
        std::unique_lock<std::mutex> lock(mutex_);
        all_req_.emplace(std::pair(req->id,  std::pair(kMemTypeQueryTradePositionReq, buffer)));
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
        all_req_.emplace(std::pair(req->id,  std::pair(kMemTypeQueryTradeKnockReq, buffer)));
    }

    void TestBroker::OnTradeOrder(MemTradeOrderMessage* req) {
#if 0
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
#endif
        // LOG_INFO << "回写报单回报";
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * req->items_size;
        char buffer[length] = "";
        memcpy(buffer, req, length);
        MemTradeOrderMessage* rep = (MemTradeOrderMessage*)buffer;
        string batch_no = "";
        std::vector<std::string> vec_order_no;
        if (rep->items_size > 1) {
            MemTradeOrder* order = req->items + 0;
            batch_no = CreateStandardBatchNo(order->market, rep->items_size, std::to_string(batch_no_index_++));
            strncpy(rep->batch_no, batch_no.c_str(), batch_no.length());
        }
        auto items = (MemTradeOrder*)((char*)rep + sizeof(MemTradeOrderMessage));
        for (int i = 0; i < rep->items_size; i++) {
            MemTradeOrder* order = items + i;
            string order_no = CreateStandardOrderNo(order->market, std::to_string(order_no_index_++));
            strcpy(order->order_no, order_no.c_str());
            vec_order_no.push_back(order_no);
        }
        if (!batch_no.empty()) {
            all_batch_.insert(std::make_pair(batch_no, vec_order_no));
        }
        SendRtnMessage(string(buffer, length), kMemTypeTradeOrderRep);
        {
            for (int i = 0; i < rep->items_size; i++) {
                MemTradeOrder* order = items + i;
                int length = sizeof(MemTradeKnock);
                char buffer[sizeof(MemTradeKnock)] = "";
                MemTradeKnock* knock = (MemTradeKnock*) buffer;
                knock->timestamp = x::RawDateTime();
                strcpy(knock->fund_id, rep->fund_id);
                strcpy(knock->batch_no, rep->batch_no);
                strcpy(knock->code, order->code);
                strcpy(knock->order_no, order->order_no);
                knock->bs_flag = rep->bs_flag;
                // 100 全部成交; 200 废单; 300 部分成交; 其它 不成交
                if (order->volume == 100) {
                    knock->match_type = co::kMatchTypeOK;
                    knock->match_volume = order->volume;
                    knock->match_price = order->price;
                    knock->match_amount = order->price * knock->match_volume;
                    string match_no = "MATCH_" + std::to_string(order_no_index_++);
                    strcpy(knock->match_no, match_no.c_str());
                } else if (order->volume == 200) {
                    knock->match_type = co::kMatchTypeFailed;
                    knock->match_volume = order->volume;
                    knock->match_price = 0;
                    knock->match_amount = 0;
                    string match_no = "_" + string(knock->order_no);
                    strcpy(knock->match_no, match_no.c_str());
                    strcpy(knock->error, "报单价格不正确");
                } else if (order->volume == 300) {
                    knock->match_type = co::kMatchTypeOK;
                    knock->match_volume = 50;
                    knock->match_price = order->price;
                    knock->match_amount = order->price * knock->match_volume;
                    string match_no = "MATCH_" + std::to_string(order_no_index_++);
                    strcpy(knock->match_no, match_no.c_str());
                    MemTradeOrder tmp = {};
                    memcpy(&tmp, order, sizeof(tmp));
                    tmp.volume = order->volume - knock->match_volume;
                    int64_t bs_flag = req->bs_flag;
                    all_order_.insert(std::make_pair(order->order_no, std::make_pair(bs_flag, tmp)));
                } else {
                    MemTradeOrder tmp = {};
                    memcpy(&tmp, order, sizeof(tmp));
                    int64_t bs_flag = req->bs_flag;
                    all_order_.insert(std::make_pair(order->order_no, std::make_pair(bs_flag, tmp)));
                    continue;
                }
                SendRtnMessage(string(buffer, length), kMemTypeTradeKnock);
            }
        }
    }

    void TestBroker::OnTradeWithdraw(MemTradeWithdrawMessage* req) {
        LOG_INFO << "req withdraw, fund_id: " << req->fund_id
                        << ", id: " << req->id
                        << ", timestamp: " << req->timestamp
                        << ", order_no: " << req->order_no
                        << ", batch_no: " << req->batch_no;
#if 0
        int length = sizeof(MemTradeWithdrawMessage);
        void* buffer = new char[length];
        memcpy(buffer, req, length);
        std::unique_lock<std::mutex> lock(mutex_);
        all_req_.emplace(std::pair(req->id, std::pair(kMemTypeTradeWithdrawReq ,buffer)));
#endif
        LOG_INFO << "回写撤单回报";
        int length = sizeof(MemTradeWithdrawMessage);
        char buffer[length] = "";
        memcpy(buffer, req, length);
        MemTradeWithdrawMessage* rep = (MemTradeWithdrawMessage*)buffer;
        if (strlen(req->order_no) > 0) {
            if (auto it = all_order_.find(req->order_no); it != all_order_.end()) {
                SendRtnMessage(string(buffer, length), kMemTypeTradeWithdrawRep);
                {
                    x::Sleep(1000);
                    int length = sizeof(MemTradeKnock);
                    char buffer[length] = "";
                    MemTradeKnock *knock = (MemTradeKnock *) buffer;
                    knock->timestamp = x::RawDateTime();
                    strcpy(knock->fund_id, rep->fund_id);
                    strcpy(knock->code, it->second.second.code);
                    strcpy(knock->order_no, rep->order_no);
                    knock->bs_flag = it->second.first;
                    knock->market = it->second.second.market;
                    knock->match_volume = it->second.second.volume;
                    knock->match_type = co::kMatchTypeWithdrawOK;
                    knock->match_price = 0;
                    knock->match_amount = 0;
                    string match_no = "_" + string(knock->order_no);
                    strcpy(knock->match_no, match_no.c_str());
                    SendRtnMessage(string(buffer, length), kMemTypeTradeKnock);
                    all_order_.erase(it);
                }
            } else {
                strcpy(rep->error, "撤单错误，报单已成交");
                SendRtnMessage(string(buffer, length), kMemTypeTradeWithdrawRep);
            }
        } else if (strlen(req->batch_no) > 0) {
            if (auto it = all_batch_.find(req->batch_no); it != all_batch_.end()) {
                SendRtnMessage(string(buffer, length), kMemTypeTradeWithdrawRep);
                {
                    for (auto& iter : it->second) {
                        if (auto it = all_order_.find(iter); it != all_order_.end()) {
                            x::Sleep(1000);
                            int length = sizeof(MemTradeKnock);
                            char buffer[length] = "";
                            MemTradeKnock *knock = (MemTradeKnock *) buffer;
                            knock->timestamp = x::RawDateTime();
                            strcpy(knock->fund_id, rep->fund_id);
                            strcpy(knock->code, it->second.second.code);
                            strcpy(knock->order_no, it->first.c_str());
                            knock->bs_flag = it->second.first;
                            knock->market = it->second.second.market;
                            knock->match_volume = it->second.second.volume;
                            knock->match_type = co::kMatchTypeWithdrawOK;
                            knock->match_price = 0;
                            knock->match_amount = 0;
                            string match_no = "_" + string(knock->order_no);
                            strcpy(knock->match_no, match_no.c_str());
                            strcpy(knock->batch_no, req->batch_no);
                            SendRtnMessage(string(buffer, length), kMemTypeTradeKnock);
                            all_order_.erase(it);
                        }
                    }
                }
                all_batch_.erase(it);
            } else {
                strcpy(rep->error, "撤单错误，报单已成交");
                SendRtnMessage(string(buffer, length), kMemTypeTradeWithdrawRep);
            }
        } else {
            strcpy(rep->error, "order_no and batch_no both empty");
            SendRtnMessage(string(buffer, length), kMemTypeTradeWithdrawRep);
        }
    }

    void TestBroker::HandReqData() {
        while (true) {
            x::Sleep(1000);
            std::unique_lock<std::mutex> lock(mutex_);
            for (auto it = all_req_.begin(); it != all_req_.end();) {
                auto& item = it->second;
                switch (item.first) {
                    case kMemTypeTradeOrderReq: {
                        break;
                    }
                    case kMemTypeTradeWithdrawReq: {
                        break;
                    }
                    case kMemTypeQueryTradeAssetReq: {
                        LOG_INFO << "查询资金响应";
                        MemGetTradeAssetMessage* req = (MemGetTradeAssetMessage*)item.second;
                        // 生成item，再判断每一个是否有效
                        int total_num = 1;
                        MemTradeAsset asset;
                        memset(&asset, 0, sizeof(asset));
                        asset.timestamp = x::RawDateTime();
                        strcpy(asset.fund_id, req->fund_id);
                        asset.balance = 100.0 + req->timestamp % 17;
                        asset.usable = 23.0;

                        int length = sizeof(MemGetTradeAssetMessage) + sizeof(MemTradeAsset) * total_num;
                        char buffer[length] = "";
                        MemGetTradeAssetMessage* rep = (MemGetTradeAssetMessage*)buffer;
                        memcpy(rep, req, sizeof(MemGetTradeAssetMessage));
                        rep->items_size = total_num;
                        if (total_num) {
                            MemTradeAsset* first = (MemTradeAsset*)((char*)buffer + sizeof(MemGetTradeAssetMessage));
                            for (int i = 0; i < total_num; i++) {
                                MemTradeAsset* item = first + i;
                                memcpy(item, &asset, sizeof(MemTradeAsset));
                            }
                        }
                        SendRtnMessage(string(buffer, length), kMemTypeQueryTradeAssetRep);
                        break;
                    }
                    case kMemTypeQueryTradePositionReq: {
                        LOG_INFO << "查询持仓响应";
                        MemGetTradePositionMessage* req = (MemGetTradePositionMessage*)item.second;
                        // 生成item，再判断每一个是否有效
                        std::vector<MemTradePosition> tmp_pos;
                        int tmp_rand =  rand();
                        for (int i = 0; i < tmp_rand % 5; i++) {
                            MemTradePosition pos;
                            memset(&pos, 0, sizeof(MemTradePosition));
                            pos.timestamp = x::RawDateTime();
                            strcpy(pos.fund_id, req->fund_id);
                            sprintf(pos.code, "00000%d.SZ", i + 1);
                            pos.market = co::kMarketSZ;
                            pos.long_volume = 200 + 100 * (tmp_rand % 13);
                            pos.long_can_close = pos.long_volume - 100;
                            tmp_pos.push_back(pos);
                        }
                        int total_num = tmp_pos.size();
                        int length = sizeof(MemGetTradePositionMessage) + sizeof(MemTradePosition) * total_num;
                        char buffer[length] = "";
                        MemGetTradePositionMessage* rep = (MemGetTradePositionMessage*)buffer;
                        memcpy(rep, req, sizeof(MemGetTradePositionMessage));
                        rep->items_size = total_num;
                        if (total_num) {
                            MemTradePosition *first = (MemTradePosition * )((char *)buffer + sizeof(MemGetTradePositionMessage));
                            for (int i = 0; i < total_num; i++) {
                                MemTradePosition *pos = first + i;
                                memcpy(pos, &tmp_pos[i], sizeof(MemTradePosition));
                            }
                        }
                        SendRtnMessage(string(buffer, length), kMemTypeQueryTradePositionRep);
                        break;
                    }
                    case kMemTypeQueryTradeKnockReq: {
                        LOG_INFO << "查询成交响应";
                        MemGetTradeKnockMessage* req = (MemGetTradeKnockMessage*)item.second;
                        std::vector<MemTradeKnock> tmp_knock;
                        for (int i = 0; i < 3; i++) {
                            MemTradeKnock knock;
                            memset(&knock, 0, sizeof(knock));
                            knock.timestamp = x::RawDateTime();
                            strcpy(knock.fund_id, req->fund_id);
                            // strcpy(knock.batch_no, "batch_no_123");
                            sprintf(knock.code, "00000%d.SZ", i + 1);
                            sprintf(knock.order_no, "order_no_%ld", knock.timestamp % 13);
                            sprintf(knock.match_no, "match_no_%ld", i + req->timestamp % 2 * 100);
                            knock.bs_flag = 1;
                            knock.match_volume = i * 100 + 100;
                            knock.match_type = 1;
                            knock.match_price = 10 + 0.01 * i;
                            knock.match_amount = knock.match_volume * knock.match_volume;
                            tmp_knock.push_back(knock);
                        }
                        int total_num = tmp_knock.size();
                        int length = sizeof(MemGetTradeKnockMessage) + sizeof(MemTradeKnock) * total_num;
                        char buffer[length] = "";
                        MemGetTradeKnockMessage* rep = (MemGetTradeKnockMessage*)buffer;
                        memcpy(rep, req, sizeof(MemGetTradeKnockMessage));
                        rep->items_size = total_num;
                        if (total_num) {
                            MemTradeKnock* first = (MemTradeKnock*)((char*)buffer + sizeof(MemGetTradeKnockMessage));
                            for (int i = 0; i < total_num; i++) {
                                MemTradeKnock* knock = first + i;;
                                memcpy(knock, &tmp_knock[i], sizeof(MemTradeKnock));
                            }
                        }
                        SendRtnMessage(string(buffer, length), kMemTypeQueryTradeKnockRep);
                        break;
                    }
                }
                delete item.second;
                item.second = nullptr;
                all_req_.erase(it);
                break;
            }
        }
    }
}  // namespace co