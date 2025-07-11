#include <string>
#include <gtest/gtest.h>

#include "../../mem_broker/utils.h"
#include "helper.h"
using namespace co;

TEST(UNITS, CheckTradeOrderMessage) {
    string id = x::UUID();
    string fund_id = "S1";
    MemTradeOrderMessage msg  = {};
    strncpy(msg.id, id.c_str(), id.length());
    strcpy(msg.fund_id, fund_id.c_str());
    msg.bs_flag = kBsFlagBuy;
    msg.items_size = 0;
    string out = CheckTradeOrderMessage(&msg, 100, 100);
    LOG_INFO << out;
    EXPECT_FALSE(out.empty());

    // 两个交易所的code
    {
        int total_order_num = 2;
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_order_num;
        char buffer[length] = {};
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*) buffer;
        strncpy(msg->id, id.c_str(), id.length());
        strcpy(msg->fund_id, fund_id.c_str());
        msg->items_size = total_order_num;
        MemTradeOrder* item = (MemTradeOrder*)((char*)buffer + sizeof(MemTradeOrderMessage));
        for (int i = 0; i < total_order_num; i++) {
            MemTradeOrder* order = item + i;
            if (i == 0) {
                strcpy(order->code, "600000.SH");
            } else {
                strcpy(order->code, "000001.SZ");
            }
            order->volume = 1000;
            order->price = 9.99;
        }
        msg->bs_flag = co::kBsFlagBuy;
        msg->timestamp = x::RawDateTime();
        string out = CheckTradeOrderMessage(msg, 100, 100);
        LOG_INFO << out;
        EXPECT_FALSE(out.empty());
    }

    {
        int total_order_num = 1;
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_order_num;
        char buffer[length] = {};
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*) buffer;
        strncpy(msg->id, id.c_str(), id.length());
        strcpy(msg->fund_id, fund_id.c_str());
        msg->items_size = total_order_num;
        MemTradeOrder* item = (MemTradeOrder*)((char*)buffer + sizeof(MemTradeOrderMessage));
        for (int i = 0; i < total_order_num; i++) {
            MemTradeOrder* order = item + i;
            strcpy(order->code, "204002.SH");
            order->volume = 1000;
            order->price = 9.99;
        }
        msg->bs_flag = co::kBsFlagSell;
        msg->timestamp = x::RawDateTime();
        string out = CheckTradeOrderMessage(msg, 100, 100);
        LOG_INFO << out;
        EXPECT_FALSE(out.empty());
    }

    {
        int total_order_num = 1;
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_order_num;
        char buffer[length] = {};
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*) buffer;
        strncpy(msg->id, id.c_str(), id.length());
        strcpy(msg->fund_id, fund_id.c_str());
        msg->items_size = total_order_num;
        MemTradeOrder* item = (MemTradeOrder*)((char*)buffer + sizeof(MemTradeOrderMessage));
        for (int i = 0; i < total_order_num; i++) {
            MemTradeOrder* order = item + i;
            strcpy(order->code, "131811.SZ");
            order->volume = 1000;
            order->price = 9.99;
        }
        msg->bs_flag = co::kBsFlagSell;
        msg->timestamp = x::RawDateTime();
        string out = CheckTradeOrderMessage(msg, 100, 100);
        LOG_INFO << out;
        EXPECT_FALSE(out.empty());
    }

    {
        int total_order_num = 101;
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_order_num;
        char buffer[length] = {};
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*) buffer;
        strncpy(msg->id, id.c_str(), id.length());
        strcpy(msg->fund_id, fund_id.c_str());
        msg->items_size = total_order_num;
        MemTradeOrder* item = (MemTradeOrder*)((char*)buffer + sizeof(MemTradeOrderMessage));
        for (int i = 0; i < total_order_num; i++) {
            MemTradeOrder* order = item + i;
            strcpy(order->code, "600000.SH");
            order->volume = 1000;
            order->price = 9.99;
        }
        msg->bs_flag = co::kBsFlagBuy;
        msg->timestamp = x::RawDateTime();
        string out = CheckTradeOrderMessage(msg, 100, 90);
        LOG_INFO << out;
        EXPECT_FALSE(out.empty());
    }
}

TEST(UNITS, CheckTradeWithdrawMessage) {
    string id = x::UUID();
    co::MemTradeWithdrawMessage msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.id, id.c_str(), id.length());
    strcpy(msg.fund_id, "S1");

    string out = CheckTradeWithdrawMessage(&msg, 1);
    LOG_INFO << out;
    EXPECT_FALSE(out.empty());

    string order_no = "1-123";
    strncpy(msg.order_no, order_no.c_str(), order_no.length());
    out = CheckTradeWithdrawMessage(&msg, 1);
    LOG_INFO << out;
    EXPECT_TRUE(out.empty());

    order_no = "a-123";
    strncpy(msg.order_no, order_no.c_str(), order_no.length());
    out = CheckTradeWithdrawMessage(&msg, 1);
    LOG_INFO << out;
    EXPECT_FALSE(out.empty());

    order_no = "10-123";
    strncpy(msg.order_no, order_no.c_str(), order_no.length());
    out = CheckTradeWithdrawMessage(&msg, 1);
    LOG_INFO << out;
    EXPECT_FALSE(out.empty());

    strcpy(msg.order_no, "");
    string batch_no = "1-999-abcd";
    strncpy(msg.batch_no, batch_no.c_str(), batch_no.length());
    out = CheckTradeWithdrawMessage(&msg, 1);
    LOG_INFO << out;
    EXPECT_TRUE(out.empty());

    strcpy(msg.batch_no, "");
    batch_no = "1-1000-abcd";
    strncpy(msg.batch_no, batch_no.c_str(), batch_no.length());
    out = CheckTradeWithdrawMessage(&msg, 1);
    LOG_INFO << out;
    EXPECT_FALSE(out.empty());

    strcpy(msg.batch_no, "");
    batch_no = "a-999-abcd";
    strncpy(msg.batch_no, batch_no.c_str(), batch_no.length());
    out = CheckTradeWithdrawMessage(&msg, 1);
    LOG_INFO << out;
    EXPECT_FALSE(out.empty());
}
