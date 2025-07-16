#include <string>
#include <gtest/gtest.h>
#include "../../mem_broker/utils.h"
#include "../../mem_broker/inner_option_master.h"

using namespace co;

string code_0 = "600000.SH";
string code_1 = "600030.SH";

TEST(InnerOptionMaster, TestSimpleOrder) {
    string fund_id = "S1";
    string id = x::UUID();
    int total_pos_num = 2;
    int long_can_close = 800;
    int short_can_close = 500;

    int length = sizeof(MemGetTradePositionMessage) + sizeof(MemTradePosition) * total_pos_num;
    char buffer[length] = "";
    MemGetTradePositionMessage* msg = (MemGetTradePositionMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id.c_str());
    msg->items_size = total_pos_num;
    for (int i = 0; i < total_pos_num; i++) {
        MemTradePosition *pos = msg->items + i;
        if (i == 0) {
            strcpy(pos->code, code_0.c_str());
        } else {
            strcpy(pos->code, code_1.c_str());
        }
        pos->long_can_close = long_can_close;
        pos->short_can_close = short_can_close;
    }
    InnerOptionMaster master;
    master.InitPositions(msg);

    // 买，数量大于short_can_close，开
    {
        int64_t bs_flag = co::kBsFlagBuy;
        MemTradeOrder order {};
        strcpy(order.code, code_0.c_str());
        order.volume = short_can_close + 100;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagOpen);
    }

    // 买，数量小于short_can_close，平
    {
        int64_t bs_flag = co::kBsFlagBuy;
        MemTradeOrder order {};
        strcpy(order.code, code_0.c_str());
        order.volume = short_can_close - 100;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagClose);
    }

    // 卖，数量大于long_can_close，开
    {
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code_1.c_str());
        order.volume = long_can_close + 100;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagOpen);
    }

    // 卖，数量小于long_can_close，平
    {
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code_1.c_str());
        order.volume = long_can_close - 100;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagClose);
    }
}

// 测试流程
// 1 卖平, 报单失败
// 2 卖平，撤单
// 3 卖平，成交
// 4 卖开

TEST(InnerOptionMaster, TestSellClose) {
    string fund_id = "S1";
    string id = x::UUID();
    int total_pos_num = 1;
    int long_can_close = 1000;
    int short_can_close = 500;
    int order_volume = 800;

    int length = sizeof(MemGetTradePositionMessage) + sizeof(MemTradePosition) * total_pos_num;
    char buffer[length] = {};
    MemGetTradePositionMessage *msg = (MemGetTradePositionMessage *) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id.c_str());
    msg->items_size = total_pos_num;
    for (int i = 0; i < total_pos_num; i++) {
        MemTradePosition *pos = msg->items + i;
        strcpy(pos->code, code_0.c_str());
        pos->long_can_close = long_can_close;
        pos->short_can_close = short_can_close;
    }
    InnerOptionMaster master;
    master.InitPositions(msg);

    int64_t bs_flag = co::kBsFlagSell;
    // 1 卖平, 报单失败
    {
        MemTradeOrder order {};
        strcpy(order.code, code_0.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagClose);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        strcpy(order.error, "报单失败");
        master.HandleOrderRep(bs_flag, order);
    }

        // 2 卖平，撤单
        {
            string order_no = CreateStandardOrderNo(kMarketSH, GenerateRandomString(3));
            string match_no = "_" + order_no;
            LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
            MemTradeOrder order {};
            strcpy(order.code, code_0.c_str());
            order.volume = order_volume;
            order.price = 9.9;
            int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
            EXPECT_EQ(oc_flag, co::kOcFlagClose);

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
            knock.match_type = co::kMatchTypeWithdrawOK;
            knock.bs_flag = bs_flag;
            knock.oc_flag = order.oc_flag;
            master.HandleKnock(knock);

            // 查看多头持仓
            InnerOptionPositionPtr long_pos = master.GetPosition(order.code, co::kBsFlagBuy);
            EXPECT_EQ(long_pos->close_volume_, 0);
            EXPECT_EQ(long_pos->closing_volume_, 0);
            EXPECT_EQ(long_pos->GetAvailableVolume(), long_can_close);
        }

    // 3 卖平，成交
    {
        string order_no = CreateStandardOrderNo(kMarketSH, GenerateRandomString(3));
        string match_no = "Trade_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        MemTradeOrder order {};
        strcpy(order.code, code_0.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagClose);
        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);

        strcpy(order.order_no, order_no.c_str());
        master.HandleOrderRep(bs_flag, order);
        InnerOptionPositionPtr long_pos = master.GetPosition(order.code, co::kBsFlagBuy);
        EXPECT_EQ(long_pos->close_volume_, 0);
        EXPECT_EQ(long_pos->closing_volume_, order_volume);
        EXPECT_EQ(long_pos->GetAvailableVolume(), long_can_close - order_volume);

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

        // 查看多头持仓
        EXPECT_EQ(long_pos->close_volume_, order_volume);
        EXPECT_EQ(long_pos->closing_volume_, 0);
        EXPECT_EQ(long_pos->GetAvailableVolume(), long_can_close - order_volume);
    }

    // 4 卖开，成交
    {
        string order_no = CreateStandardOrderNo(kMarketSH, GenerateRandomString(3));
        string match_no = "Trade_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        MemTradeOrder order {};
        strcpy(order.code, code_0.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
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

        // 查看空头持仓
        InnerOptionPositionPtr long_pos = master.GetPosition(order.code, co::kBsFlagSell);
        EXPECT_EQ(long_pos->open_volume_, order_volume);
        EXPECT_EQ(long_pos->GetAvailableVolume(), short_can_close + order_volume);
    }
}

// 测试流程
// 1 买平, 报单失败
// 2 买平，撤单
// 3 买平，成交
// 4 买开，成交

TEST(InnerOptionMaster, TestBuyClose) {
    string fund_id = "S1";
    string id = x::UUID();
    int total_pos_num = 1;
    int long_can_close = 1000;
    int short_can_close = 500;
    int order_volume = 400;

    int length = sizeof(MemGetTradePositionMessage) + sizeof(MemTradePosition) * total_pos_num;
    char buffer[length] = {};
    MemGetTradePositionMessage *msg = (MemGetTradePositionMessage *) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id.c_str());
    msg->items_size = total_pos_num;
    for (int i = 0; i < total_pos_num; i++) {
        MemTradePosition *pos = msg->items + i;
        strcpy(pos->code, code_0.c_str());
        pos->long_can_close = long_can_close;
        pos->short_can_close = short_can_close;
    }
    InnerOptionMaster master;
    master.InitPositions(msg);

    int64_t bs_flag = co::kBsFlagBuy;
    // 1 买平, 报单失败
    {
        MemTradeOrder order {};
        strcpy(order.code, code_0.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagClose);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        strcpy(order.error, "报单失败");
        master.HandleOrderRep(bs_flag, order);
    }

    // 2 买平，撤单
    {
        string order_no = CreateStandardOrderNo(kMarketSH, GenerateRandomString(3));
        string match_no = "Trade_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        MemTradeOrder order {};
        strcpy(order.code, code_0.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagClose);

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
        knock.match_type = co::kMatchTypeWithdrawOK;
        knock.bs_flag = bs_flag;
        knock.oc_flag = order.oc_flag;
        master.HandleKnock(knock);

        // 查看空头持仓
        InnerOptionPositionPtr long_pos = master.GetPosition(order.code, co::kBsFlagSell);
        EXPECT_EQ(long_pos->close_volume_, 0);
        EXPECT_EQ(long_pos->closing_volume_, 0);
        EXPECT_EQ(long_pos->GetAvailableVolume(), short_can_close);
    }

    // 3 买平，成交
    {
        string order_no = CreateStandardOrderNo(kMarketSH, GenerateRandomString(3));
        string match_no = "Trade_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        MemTradeOrder order {};
        strcpy(order.code, code_0.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagClose);
        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);

        strcpy(order.order_no, order_no.c_str());
        master.HandleOrderRep(bs_flag, order);
        InnerOptionPositionPtr long_pos = master.GetPosition(order.code, co::kBsFlagSell);
        EXPECT_EQ(long_pos->close_volume_, 0);
        EXPECT_EQ(long_pos->closing_volume_, order_volume);
        EXPECT_EQ(long_pos->GetAvailableVolume(), short_can_close - order_volume);

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

        // 查看空头持仓
        EXPECT_EQ(long_pos->close_volume_, order_volume);
        EXPECT_EQ(long_pos->closing_volume_, 0);
        EXPECT_EQ(long_pos->GetAvailableVolume(), short_can_close - order_volume);
    }

    // 4 买开，成交
    {
        string order_no = CreateStandardOrderNo(kMarketSH, GenerateRandomString(3));
        string match_no = "Trade_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        MemTradeOrder order {};
        strcpy(order.code, code_0.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
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

        // 查看多头持仓
        InnerOptionPositionPtr long_pos = master.GetPosition(order.code, co::kBsFlagBuy);
        EXPECT_EQ(long_pos->open_volume_, order_volume);
        EXPECT_EQ(long_pos->GetAvailableVolume(), long_can_close + order_volume);
    }
}

