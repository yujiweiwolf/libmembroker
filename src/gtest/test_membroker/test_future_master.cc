#include <string>
#include <gtest/gtest.h>
#include "../../mem_broker/utils.h"
#include "../../mem_broker/inner_future_master.h"

using namespace co;

TEST(InnerFutureMaster, TestSHFE) {
    string fund_id = "S1";
    string code = "cu2508.SHFE";
    // string code = "sc2508.INE";
    string id = x::UUID();
    int total_pos_num = 1;
    int long_volume = 8;
    int long_pre_volume = 3;

    int length = sizeof(MemGetTradePositionMessage) + sizeof(MemTradePosition) * total_pos_num;
    char buffer[length] = "";
    MemGetTradePositionMessage* msg = (MemGetTradePositionMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id.c_str());
    msg->items_size = total_pos_num;
    for (int i = 0; i < total_pos_num; i++) {
        MemTradePosition *pos = msg->items + i;
        strcpy(pos->code, code.c_str());
        pos->long_volume = long_volume;
        pos->long_pre_volume = long_pre_volume;
    }
    InnerFutureMaster master;
    master.InitPositions(msg);  // 昨仓3手, 今仓5手

    // 卖，数量小于昨仓，平昨
    {
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = long_pre_volume - 1;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagCloseYesterday);
    }

    // 卖，大于昨仓，小于今仓, 平今
    {
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = 4;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagCloseToday);
    }

    // 卖，大于昨仓，大于今仓, 平今，开
    {
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = 6;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagOpen);
    }
}

// 测试流程
// 1 卖平, 报单失败
// 2 卖平，撤单
// 3 卖平，成交
// 4 卖开

TEST(InnerFutureMaster, TestSHFEOrder) {
    string fund_id = "S1";
    string code = "cu2508.SHFE";
    string id = x::UUID();
    int total_pos_num = 1;
    int long_volume = 8;
    int long_pre_volume = 3;

    int length = sizeof(MemGetTradePositionMessage) + sizeof(MemTradePosition) * total_pos_num;
    char buffer[length] = "";
    MemGetTradePositionMessage* msg = (MemGetTradePositionMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id.c_str());
    msg->items_size = total_pos_num;
    for (int i = 0; i < total_pos_num; i++) {
        MemTradePosition *pos = msg->items + i;
        strcpy(pos->code, code.c_str());
        pos->long_volume = long_volume;
        pos->long_pre_volume = long_pre_volume;
    }
    InnerFutureMaster master;
    master.InitPositions(msg);  // 昨仓3手, 今仓5手

    // 卖，数量小于昨仓，平昨
    {
        int order_volume = 2;
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagCloseYesterday);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        strcpy(order.error, "报单失败");
        master.HandleOrderRep(bs_flag, order);
    }
}


