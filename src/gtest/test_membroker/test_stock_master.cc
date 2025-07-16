#include <string>
#include <gtest/gtest.h>

#include "../../mem_broker/utils.h"
#include "../../mem_broker/inner_stock_master.h"
#include "helper.h"
using namespace co;

string fund_id = "S1";
// 测试流程
// 1 普通卖, 报单失败
// 2 普通卖，撤单
// 3 普通卖，成交
// 4 普通买，成交
// 5 融券卖，成交

TEST(InnerStockMaster, CommonTrade) {
    string id = x::UUID();
    int total_pos_num = 1;
    int init_sell_volume = 1000;
    int init_borrowed_volume = 500;
    int order_volume = 800;
    string code = "600000.SH";
    int64_t bs_flag = co::kBsFlagSell;

    int length = sizeof(MemGetTradePositionMessage) + sizeof(MemTradePosition) * total_pos_num;
    char buffer[length] = {};
    MemGetTradePositionMessage *msg = (MemGetTradePositionMessage *) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id.c_str());
    msg->items_size = total_pos_num;
    for (int i = 0; i < total_pos_num; i++) {
        MemTradePosition *pos = msg->items + i;
        strcpy(pos->code, code.c_str());
        pos->long_can_close = init_sell_volume;
        pos->short_can_open = init_borrowed_volume;
    }
    InnerStockMaster master;
    master.SetInitPositions(msg);
    InnerStockPositionPtr pos = master.GetPosition(code);
    // 1 普通卖, 报单失败
    {
        MemTradeOrder order{};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagAuto);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        EXPECT_EQ(pos->selling_volume_, order_volume);
        EXPECT_EQ(pos->sold_volume_, 0);

        strcpy(order.error, "报单失败");
        master.HandleOrderRep(bs_flag, order);

        EXPECT_EQ(pos->selling_volume_, 0);
        EXPECT_EQ(pos->sold_volume_, 0);
    }

    // 2 普通卖，撤单
    {
        string order_no = CreateStandardOrderNo(kMarketSH, GenerateRandomString(3));
        string match_no = "_" + order_no;
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagAuto);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        strcpy(order.order_no, order_no.c_str());
        master.HandleOrderRep(bs_flag, order);
        EXPECT_EQ(pos->selling_volume_, order_volume);
        EXPECT_EQ(pos->sold_volume_, 0);

        MemTradeKnock knock {};
        strcpy(knock.fund_id, fund_id.c_str());
        strcpy(knock.code, order.code);
        strcpy(knock.order_no, order.order_no);
        strcpy(knock.inner_match_no, match_no.c_str());
        knock.match_volume = order.volume;
        knock.match_type = co::kMatchTypeWithdrawOK;
        knock.bs_flag = bs_flag;
        knock.oc_flag = order.oc_flag;
        master.HandleKnock(knock);

        EXPECT_EQ(pos->selling_volume_, 0);
        EXPECT_EQ(pos->sold_volume_, 0);
    }

    // 3 普通卖，成交
    {
        string order_no = CreateStandardOrderNo(kMarketSH, GenerateRandomString(3));
        string match_no = "Trade_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagAuto);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        strcpy(order.order_no, order_no.c_str());
        master.HandleOrderRep(bs_flag, order);

        MemTradeKnock knock{};
        strcpy(knock.fund_id, fund_id.c_str());
        strcpy(knock.code, order.code);
        strcpy(knock.order_no, order.order_no);
        strcpy(knock.inner_match_no, match_no.c_str());
        knock.match_volume = order.volume;
        knock.match_type = co::kMatchTypeOK;
        knock.bs_flag = bs_flag;
        knock.oc_flag = order.oc_flag;
        master.HandleKnock(knock);

        EXPECT_EQ(pos->selling_volume_, 0);
        EXPECT_EQ(pos->sold_volume_, order_volume);
        int64_t available_volume = pos->init_sell_volume_ - pos->selling_volume_ - pos->sold_volume_;
        EXPECT_EQ(available_volume, 200);
    }

    // 4 普通买，成交
    {
        string order_no = CreateStandardOrderNo(kMarketSH, GenerateRandomString(3));
        string match_no = "Trade_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int64_t bs_flag = kBsFlagBuy;
        MemTradeOrder order{};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagAuto);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        strcpy(order.order_no, order_no.c_str());
        master.HandleOrderRep(bs_flag, order);

        MemTradeKnock knock{};
        strcpy(knock.fund_id, fund_id.c_str());
        strcpy(knock.code, order.code);
        strcpy(knock.order_no, order.order_no);
        strcpy(knock.inner_match_no, match_no.c_str());
        knock.match_volume = order.volume;
        knock.match_type = co::kMatchTypeOK;
        knock.bs_flag = bs_flag;
        knock.oc_flag = order.oc_flag;
        master.HandleKnock(knock);

        EXPECT_EQ(pos->selling_volume_, 0);
        EXPECT_EQ(pos->sold_volume_, order_volume);
        int64_t available_volume = pos->init_sell_volume_ - pos->selling_volume_ - pos->sold_volume_;
        EXPECT_EQ(available_volume, 200);
    }

    // 5 融券卖，成交
    {
        string order_no = CreateStandardOrderNo(kMarketSH, GenerateRandomString(3));
        string match_no = "Trade_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int64_t new_order_volume = 300;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = new_order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagOpen);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        strcpy(order.order_no, order_no.c_str());
        master.HandleOrderRep(bs_flag, order);

        MemTradeKnock knock {};
        strcpy(knock.fund_id, fund_id.c_str());
        strcpy(knock.code, order.code);
        strcpy(knock.order_no, order.order_no);
        strcpy(knock.inner_match_no, match_no.c_str());
        knock.match_volume = order.volume;
        knock.match_type = co::kMatchTypeOK;
        knock.bs_flag = bs_flag;
        knock.oc_flag = order.oc_flag;
        master.HandleKnock(knock);

        EXPECT_EQ(pos->selling_volume_, 0);
        EXPECT_EQ(pos->sold_volume_, order_volume);
        EXPECT_EQ(pos->borrowing_volume_, 0);
        EXPECT_EQ(pos->borrowed_volume_, new_order_volume);
        int64_t available_volume = pos->init_sell_volume_ - pos->selling_volume_ - pos->sold_volume_;
        EXPECT_EQ(available_volume, 200);
    }
}


// 测试流程
// 1 普通卖, 报单失败
// 2 普通卖，撤单
// 3 普通卖，成交
// 4 普通买，成交
// 5 普通卖，成交 (4 买的，可以正常卖)
// 5 融券卖，成交

TEST(InnerStockMaster, T0Trade) {
    string id = x::UUID();
    int total_pos_num = 1;
    int init_sell_volume = 1000;
    int init_borrowed_volume = 500;
    int order_volume = 800;
    string code = "513050.SH";
    int64_t bs_flag = co::kBsFlagSell;

    int length = sizeof(MemGetTradePositionMessage) + sizeof(MemTradePosition) * total_pos_num;
    char buffer[length] = {};
    MemGetTradePositionMessage *msg = (MemGetTradePositionMessage *) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id.c_str());
    msg->items_size = total_pos_num;
    for (int i = 0; i < total_pos_num; i++) {
        MemTradePosition *pos = msg->items + i;
        strcpy(pos->code, code.c_str());
        pos->long_can_close = init_sell_volume;
        pos->short_can_open = init_borrowed_volume;
    }
    InnerStockMaster master;
    master.SetInitPositions(msg);
    master.AddT0Code(code);
    InnerStockPositionPtr pos = master.GetPosition(code);
    // 1 普通卖, 报单失败
    {
        MemTradeOrder order{};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagAuto);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        EXPECT_EQ(pos->selling_volume_, order_volume);
        EXPECT_EQ(pos->sold_volume_, 0);

        strcpy(order.error, "报单失败");
        master.HandleOrderRep(bs_flag, order);

        EXPECT_EQ(pos->selling_volume_, 0);
        EXPECT_EQ(pos->sold_volume_, 0);
    }

    // 2 普通卖，撤单
    {
        string order_no = CreateStandardOrderNo(kMarketSH, GenerateRandomString(3));
        string match_no = "_" + order_no;
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagAuto);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        strcpy(order.order_no, order_no.c_str());
        master.HandleOrderRep(bs_flag, order);
        EXPECT_EQ(pos->selling_volume_, order_volume);
        EXPECT_EQ(pos->sold_volume_, 0);

        MemTradeKnock knock {};
        strcpy(knock.fund_id, fund_id.c_str());
        strcpy(knock.code, order.code);
        strcpy(knock.order_no, order.order_no);
        strcpy(knock.inner_match_no, match_no.c_str());
        knock.match_volume = order.volume;
        knock.match_type = co::kMatchTypeWithdrawOK;
        knock.bs_flag = bs_flag;
        knock.oc_flag = order.oc_flag;
        master.HandleKnock(knock);

        EXPECT_EQ(pos->selling_volume_, 0);
        EXPECT_EQ(pos->sold_volume_, 0);
    }

    // 3 普通卖，成交
    {
        string order_no = CreateStandardOrderNo(kMarketSH, GenerateRandomString(3));
        string match_no = "Trade_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagAuto);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        strcpy(order.order_no, order_no.c_str());
        master.HandleOrderRep(bs_flag, order);

        MemTradeKnock knock{};
        strcpy(knock.fund_id, fund_id.c_str());
        strcpy(knock.code, order.code);
        strcpy(knock.order_no, order.order_no);
        strcpy(knock.inner_match_no, match_no.c_str());
        knock.match_volume = order.volume;
        knock.match_type = co::kMatchTypeOK;
        knock.bs_flag = bs_flag;
        knock.oc_flag = order.oc_flag;
        master.HandleKnock(knock);

        EXPECT_EQ(pos->selling_volume_, 0);
        EXPECT_EQ(pos->sold_volume_, order_volume);
        int64_t available_volume = pos->init_sell_volume_ - pos->selling_volume_ - pos->sold_volume_;
        EXPECT_EQ(available_volume, 200);
    }

    // 4 普通买，成交
    {
        string order_no = CreateStandardOrderNo(kMarketSH, GenerateRandomString(3));
        string match_no = "Trade_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int64_t bs_flag = kBsFlagBuy;
        MemTradeOrder order{};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagAuto);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        strcpy(order.order_no, order_no.c_str());
        master.HandleOrderRep(bs_flag, order);

        MemTradeKnock knock{};
        strcpy(knock.fund_id, fund_id.c_str());
        strcpy(knock.code, order.code);
        strcpy(knock.order_no, order.order_no);
        strcpy(knock.inner_match_no, match_no.c_str());
        knock.match_volume = order.volume;
        knock.match_type = co::kMatchTypeOK;
        knock.bs_flag = bs_flag;
        knock.oc_flag = order.oc_flag;
        master.HandleKnock(knock);

        EXPECT_EQ(pos->selling_volume_, 0);
        EXPECT_EQ(pos->sold_volume_, order_volume);
        EXPECT_EQ(pos->bought_volume_, order_volume);
        int64_t available_volume = pos->init_sell_volume_ + pos->bought_volume_ - pos->selling_volume_ - pos->sold_volume_;
        EXPECT_EQ(available_volume, 1000);
    }

    // 5 普通卖，成交
    {
        string order_no = CreateStandardOrderNo(kMarketSH, GenerateRandomString(3));
        string match_no = "Trade_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int64_t new_order_volume = 700;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = new_order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagAuto);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        strcpy(order.order_no, order_no.c_str());
        master.HandleOrderRep(bs_flag, order);

        MemTradeKnock knock {};
        strcpy(knock.fund_id, fund_id.c_str());
        strcpy(knock.code, order.code);
        strcpy(knock.order_no, order.order_no);
        strcpy(knock.inner_match_no, match_no.c_str());
        knock.match_volume = order.volume;
        knock.match_type = co::kMatchTypeOK;
        knock.bs_flag = bs_flag;
        knock.oc_flag = order.oc_flag;
        master.HandleKnock(knock);

        EXPECT_EQ(pos->selling_volume_, 0);
        EXPECT_EQ(pos->sold_volume_, order_volume + new_order_volume);
        EXPECT_EQ(pos->borrowing_volume_, 0);
        EXPECT_EQ(pos->borrowed_volume_, 0);
        int64_t available_volume = pos->init_sell_volume_ + pos->bought_volume_ - pos->selling_volume_ - pos->sold_volume_;
        EXPECT_EQ(available_volume, 300);
    }

    // 6 融券卖，成交
    {
        string order_no = CreateStandardOrderNo(kMarketSH, GenerateRandomString(3));
        string match_no = "Trade_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int64_t new_order_volume = 400;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = new_order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagOpen);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        strcpy(order.order_no, order_no.c_str());
        master.HandleOrderRep(bs_flag, order);

        MemTradeKnock knock {};
        strcpy(knock.fund_id, fund_id.c_str());
        strcpy(knock.code, order.code);
        strcpy(knock.order_no, order.order_no);
        strcpy(knock.inner_match_no, match_no.c_str());
        knock.match_volume = order.volume;
        knock.match_type = co::kMatchTypeOK;
        knock.bs_flag = bs_flag;
        knock.oc_flag = order.oc_flag;
        master.HandleKnock(knock);

        EXPECT_EQ(pos->borrowing_volume_, 0);
        EXPECT_EQ(pos->borrowed_volume_, new_order_volume);
        // 融券卖, 可用不变
        int64_t available_volume = pos->init_sell_volume_ + pos->bought_volume_ - pos->selling_volume_ - pos->sold_volume_;
        EXPECT_EQ(available_volume, 300);
    }
}

