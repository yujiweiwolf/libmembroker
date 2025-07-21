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
// 0 初始化, 昨仓3手, 今仓5手
// 1 卖平2手, 平昨, 报单失败
// 2 卖平2手, 平昨，撤单
// 3 卖平2手, 平昨，成交
// 4 卖平4手, 平今，成交
// 5 卖2手, 此时昨1手，今1手，卖开

TEST(InnerFutureMaster, TestSHFEOrder) {
    string fund_id = "S1";
    string code = "cu2508.SHFE";
    string id = x::UUID();
    int total_pos_num = 1;
    int long_volume = 8;
    int long_pre_volume = 3;
    int long_today_volume = long_volume - long_pre_volume;

    // 0 初始化, 昨仓3手, 今仓5手
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
    master.InitPositions(msg);

    // 1 卖平2手, 平昨, 报单失败
    {
        LOG_INFO << "流程1 卖平2手, 平昨, 报单失败";
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
        // 查看昨持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->yd_closing_volume_, 0);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), long_pre_volume);
    }

    // 2 卖平2手, 平昨，撤单
    {
        LOG_INFO << "流程2 卖平2手, 平昨，撤单";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "_" + order_no;
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
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
        // 查看昨持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->yd_closing_volume_, order_volume);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), long_pre_volume - order_volume);
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

        // 查看昨持仓
        EXPECT_EQ(long_pos->yd_closing_volume_, 0);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), long_pre_volume);
    }

    // 3 卖平2手, 平昨，成交
    {
        LOG_INFO << "流程3 卖平2手, 平昨，成交";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "FUTURE_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
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
        // 查看昨持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->yd_closing_volume_, order_volume);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), long_pre_volume - order_volume);
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

        // 查看昨持仓
        EXPECT_EQ(long_pos->yd_closing_volume_, 0);
        EXPECT_EQ(long_pos->yd_close_volume_, order_volume);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), long_pre_volume - order_volume);
    }

    // 4 卖平4手, 平今，成交
    {
        LOG_INFO << "流程4 卖平4手, 平今，成交";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "FUTURE_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int order_volume = 4;
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagCloseToday);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看今持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->td_closing_volume_, order_volume);
        EXPECT_EQ(long_pos->td_close_volume_, 0);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), long_today_volume - order_volume);
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

        // 查看今持仓
        EXPECT_EQ(long_pos->td_closing_volume_, 0);
        EXPECT_EQ(long_pos->td_close_volume_, order_volume);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), long_today_volume - order_volume);
    }
    // 5 卖2手, 此时昨1手，今1手，卖开
    {
        LOG_INFO << "流程5 卖2手, 此时昨1手，今1手，卖开";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "FUTURE_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int order_volume = 2;
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagOpen);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看今持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->td_opening_volume_, order_volume);
        EXPECT_EQ(long_pos->td_open_volume_, 0);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), 0);
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

        // 查看今持仓
        EXPECT_EQ(long_pos->td_opening_volume_, 0);
        EXPECT_EQ(long_pos->td_open_volume_, order_volume);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), order_volume);
    }
}

// INE与SHFE的开平逻辑完全一样, 测试用例的也一样，买卖方向相反
// 测试流程
// 0 初始化, 昨仓3手, 今仓5手
// 1 买平2手, 平昨, 报单失败
// 2 买平2手, 平昨，撤单
// 3 买平2手, 平昨，成交
// 4 买平4手, 平今，成交
// 5 买2手, 此时昨1手，今1手，买开

TEST(InnerFutureMaster, TestINEOrder) {
    string fund_id = "S1";
    string code = "sc2508.INE";
    string id = x::UUID();
    int total_pos_num = 1;
    int short_volume = 8;
    int short_pre_volume = 3;
    int short_today_volume = short_volume - short_pre_volume;

    // 0 初始化, 昨仓3手, 今仓5手
    int length = sizeof(MemGetTradePositionMessage) + sizeof(MemTradePosition) * total_pos_num;
    char buffer[length] = "";
    MemGetTradePositionMessage* msg = (MemGetTradePositionMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id.c_str());
    msg->items_size = total_pos_num;
    for (int i = 0; i < total_pos_num; i++) {
        MemTradePosition *pos = msg->items + i;
        strcpy(pos->code, code.c_str());
        pos->short_volume = short_volume;
        pos->short_pre_volume = short_pre_volume;
    }
    InnerFutureMaster master;
    master.InitPositions(msg);

    // 1 买平2手, 平昨, 报单失败
    {
        LOG_INFO << "流程1 买平2手, 平昨, 报单失败";
        int order_volume = 2;
        int64_t bs_flag = co::kBsFlagBuy;
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
        // 查看昨持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->yd_closing_volume_, 0);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), short_pre_volume);
    }

    // 2 买平2手, 平昨，撤单
    {
        LOG_INFO << "流程2 买平2手, 平昨，撤单";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "_" + order_no;
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int order_volume = 2;
        int64_t bs_flag = co::kBsFlagBuy;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagCloseYesterday);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看昨持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->yd_closing_volume_, order_volume);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), short_pre_volume - order_volume);
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

        // 查看昨持仓
        EXPECT_EQ(long_pos->yd_closing_volume_, 0);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), short_pre_volume);
    }

    // 3 买平2手, 平昨，成交
    {
        LOG_INFO << "流程3 买平2手, 平昨，成交";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "FUTURE_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int order_volume = 2;
        int64_t bs_flag = co::kBsFlagBuy;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagCloseYesterday);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看昨持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->yd_closing_volume_, order_volume);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), short_pre_volume - order_volume);
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

        // 查看昨持仓
        EXPECT_EQ(long_pos->yd_closing_volume_, 0);
        EXPECT_EQ(long_pos->yd_close_volume_, order_volume);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), short_pre_volume - order_volume);
    }

    // 4 买平4手, 平今，成交
    {
        LOG_INFO << "流程4 买平4手, 平今，成交";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "FUTURE_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int order_volume = 4;
        int64_t bs_flag = co::kBsFlagBuy;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagCloseToday);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看今持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->td_closing_volume_, order_volume);
        EXPECT_EQ(long_pos->td_close_volume_, 0);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), short_today_volume - order_volume);
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

        // 查看今持仓
        EXPECT_EQ(long_pos->td_closing_volume_, 0);
        EXPECT_EQ(long_pos->td_close_volume_, order_volume);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), short_today_volume - order_volume);
    }
    // 5 买2手, 此时昨1手，今1手，卖开
    {
        LOG_INFO << "流程5 买2手, 此时昨1手，今1手，买开";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "FUTURE_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int order_volume = 2;
        int64_t bs_flag = co::kBsFlagBuy;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagOpen);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看今持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->td_opening_volume_, order_volume);
        EXPECT_EQ(long_pos->td_open_volume_, 0);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), 0);
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

        // 查看今持仓
        EXPECT_EQ(long_pos->td_opening_volume_, 0);
        EXPECT_EQ(long_pos->td_open_volume_, order_volume);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), order_volume);
    }
}


// 测试流程, CZCE, 先开先平, 先平今，后平昨
// 0 初始化, 昨仓3手, 今仓5手
// 1 卖6手, 平, 报单失败
// 2 卖6手, 平，撤单
// 3 卖6手, 平，成交
// 4 卖2手, 平，成交
// 5 卖2手, 开

TEST(InnerFutureMaster, TestCZCEOrder) {
    string fund_id = "S1";
    string code = "SR2508.CZCE";
    string id = x::UUID();
    int total_pos_num = 1;
    int long_volume = 8;
    int long_pre_volume = 3;
    int long_today_volume = long_volume - long_pre_volume;

    // 0 初始化, 昨仓3手, 今仓5手
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
    master.InitPositions(msg);

    // 1 卖6手, 平, 报单失败
    {
        LOG_INFO << "流程1 卖6手, 平, 报单失败";
        int order_volume = 6;
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagClose);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看多头持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->td_closing_volume_, long_today_volume);
        EXPECT_EQ(long_pos->td_close_volume_, 0);
        EXPECT_EQ(long_pos->yd_closing_volume_, order_volume - long_today_volume);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), 2);

        strcpy(order.error, "报单失败");
        master.HandleOrderRep(bs_flag, order);
        // 查看多头持仓
        EXPECT_EQ(long_pos->td_closing_volume_, 0);
        EXPECT_EQ(long_pos->td_close_volume_, 0);
        EXPECT_EQ(long_pos->yd_closing_volume_, 0);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), long_today_volume);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), long_pre_volume);
    }

    // 2 卖6手, 平，撤单
    {
        LOG_INFO << "流程2 卖6手, 平，撤单";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "_" + order_no;
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int order_volume = 6;
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagClose);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看多头持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->td_closing_volume_, long_today_volume);
        EXPECT_EQ(long_pos->td_close_volume_, 0);
        EXPECT_EQ(long_pos->yd_closing_volume_, order_volume - long_today_volume);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), 2);

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
        EXPECT_EQ(long_pos->td_closing_volume_, 0);
        EXPECT_EQ(long_pos->td_close_volume_, 0);
        EXPECT_EQ(long_pos->yd_closing_volume_, 0);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), long_today_volume);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), long_pre_volume);
    }

    // 3 卖6手, 平，成交
    {
        LOG_INFO << "流程3 卖平2手, 平昨，成交";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "FUTURE_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int order_volume = 6;
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagClose);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看多头持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->td_closing_volume_, long_today_volume);
        EXPECT_EQ(long_pos->td_close_volume_, 0);
        EXPECT_EQ(long_pos->yd_closing_volume_, order_volume - long_today_volume);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), 2);

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
        EXPECT_EQ(long_pos->td_closing_volume_, 0);
        EXPECT_EQ(long_pos->td_close_volume_, long_today_volume);
        EXPECT_EQ(long_pos->yd_closing_volume_, 0);
        EXPECT_EQ(long_pos->yd_close_volume_, order_volume - long_today_volume);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), 2);
    }

    // 4 卖2手, 平，成交
    {
        LOG_INFO << "流程4 卖2手, 平，成交";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "FUTURE_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int order_volume = 2;
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagClose);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看多头持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->td_closing_volume_, 0);
        EXPECT_EQ(long_pos->td_close_volume_, 5);
        EXPECT_EQ(long_pos->yd_closing_volume_, 2);
        EXPECT_EQ(long_pos->yd_close_volume_, 1);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), 0);

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

        // 查看今持仓
        EXPECT_EQ(long_pos->td_closing_volume_, 0);
        EXPECT_EQ(long_pos->td_close_volume_, 5);
        EXPECT_EQ(long_pos->yd_closing_volume_, 0);
        EXPECT_EQ(long_pos->yd_close_volume_, 3);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), 0);
    }

    // 5 卖2手, 开
    {
        LOG_INFO << "流程5 卖2手, 开";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "FUTURE_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int order_volume = 2;
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagOpen);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看今持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->td_opening_volume_, order_volume);
        EXPECT_EQ(long_pos->td_open_volume_, 0);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), 0);
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

        // 查看今持仓
        EXPECT_EQ(long_pos->td_opening_volume_, 0);
        EXPECT_EQ(long_pos->td_open_volume_, order_volume);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), order_volume);
    }
}

// 测试流程, CFFEX, 只能平昨, 不能平今; 今天品种有过开仓，就不能平它的昨仓
// 0 初始化, 昨仓3手
// 1 卖2手, 平, 报单失败
// 2 卖2手, 平，撤单
// 3 卖2手, 平，成交
// 4 卖2手, 开，成交
// 5 买2手, 开, 锁仓
// 6 此时还有一手昨仓, 卖1手, 已开过仓，不允许继续平仓，只能继续开仓

TEST(InnerFutureMaster, TestCFFEXEOrder) {
    string fund_id = "S1";
    string code = "IF2508.CFFEX";
    string id = x::UUID();
    int total_pos_num = 1;
    int long_volume = 3;
    int long_pre_volume = 3;
    int long_today_volume = long_volume - long_pre_volume;

    // 0 初始化, 昨仓3手
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
    master.InitPositions(msg);

    // 1 卖2手, 平, 报单失败
    {
        LOG_INFO << "流程1 卖2手, 平, 报单失败";
        int order_volume = 2;
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagClose);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看多头持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->td_closing_volume_, 0);
        EXPECT_EQ(long_pos->td_close_volume_, 0);
        EXPECT_EQ(long_pos->yd_closing_volume_, 2);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), 1);

        strcpy(order.error, "报单失败");
        master.HandleOrderRep(bs_flag, order);
        // 查看多头持仓
        EXPECT_EQ(long_pos->td_closing_volume_, 0);
        EXPECT_EQ(long_pos->td_close_volume_, 0);
        EXPECT_EQ(long_pos->yd_closing_volume_, 0);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), long_today_volume);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), long_pre_volume);
    }

    // 2 卖2手, 平，撤单
    {
        LOG_INFO << "流程2 卖2手, 平，撤单";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "_" + order_no;
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int order_volume = 2;
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagClose);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看多头持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->td_closing_volume_, 0);
        EXPECT_EQ(long_pos->td_close_volume_, 0);
        EXPECT_EQ(long_pos->yd_closing_volume_, order_volume);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), long_pre_volume - order_volume);

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
        EXPECT_EQ(long_pos->td_closing_volume_, 0);
        EXPECT_EQ(long_pos->td_close_volume_, 0);
        EXPECT_EQ(long_pos->yd_closing_volume_, 0);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), long_today_volume);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), long_pre_volume);
    }

    // 3 卖2手, 平，成交
    {
        LOG_INFO << "流程3 卖2手, 平，成交";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "FUTURE_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int order_volume = 2;
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagClose);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看多头持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->td_closing_volume_, 0);
        EXPECT_EQ(long_pos->td_close_volume_, 0);
        EXPECT_EQ(long_pos->yd_closing_volume_, order_volume);
        EXPECT_EQ(long_pos->yd_close_volume_, 0);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), long_pre_volume - order_volume);

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
        EXPECT_EQ(long_pos->td_closing_volume_, 0);
        EXPECT_EQ(long_pos->td_close_volume_, 0);
        EXPECT_EQ(long_pos->yd_closing_volume_, 0);
        EXPECT_EQ(long_pos->yd_close_volume_, order_volume);
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), long_pre_volume - order_volume);
    }

    // 4卖2手, 开，成交
    {
        LOG_INFO << "流程4 卖2手, 开，成交";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "FUTURE_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int order_volume = 2;
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagOpen);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看空头持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->td_opening_volume_, order_volume);
        EXPECT_EQ(long_pos->td_open_volume_, 0);

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

        // 查看今持仓
        EXPECT_EQ(long_pos->td_opening_volume_, 0);
        EXPECT_EQ(long_pos->td_open_volume_, order_volume);
    }

    // 5 买2手, 开, 锁仓, 此时还有一手昨仓
    {
        LOG_INFO << "流程5 买2手, 开, 锁仓";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "FUTURE_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int order_volume = 2;
        int64_t bs_flag = co::kBsFlagBuy;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagOpen);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看多头持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(long_pos->td_opening_volume_, order_volume);
        EXPECT_EQ(long_pos->td_open_volume_, 0);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), 1);  // 此时还有一手昨仓
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
        EXPECT_EQ(long_pos->td_opening_volume_, 0);
        EXPECT_EQ(long_pos->td_open_volume_, order_volume);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), 1);  // 此时还有一手昨仓
        EXPECT_EQ(long_pos->GetTodayAvailableVolume(), order_volume);
    }

    // 6 此时还有一手昨仓, 卖1手, 已开过仓，不允许继续平仓，只能继续开仓
    {
        LOG_INFO << "流程6 此时还有一手昨仓, 卖1手, 已开过仓，不允许继续平仓，只能继续开仓";
        string order_no = CreateStandardOrderNo(kMarketSHFE, GenerateRandomString(3));
        string match_no = "FUTURE_" + GenerateRandomString(5);
        LOG_INFO << "order_no: " << order_no << ", match_no: " << match_no << " ------------------------";
        int order_volume = 1;
        int64_t bs_flag = co::kBsFlagSell;
        MemTradeOrder order {};
        strcpy(order.code, code.c_str());
        order.volume = order_volume;
        order.price = 9.9;
        int64_t oc_flag = master.GetAutoOcFlag(bs_flag, order);
        EXPECT_EQ(oc_flag, co::kOcFlagOpen);

        order.oc_flag = oc_flag;
        master.HandleOrderReq(bs_flag, order);
        // 查看空头持仓
        InnerFuturePositionPtr short_pos = master.GetPosition(order.code, bs_flag, order.oc_flag);
        EXPECT_EQ(short_pos->td_opening_volume_, order_volume);
        EXPECT_EQ(short_pos->td_open_volume_, 2);
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
        EXPECT_EQ(short_pos->td_opening_volume_, 0);
        EXPECT_EQ(short_pos->td_open_volume_, 3);

        // 查看多头持仓
        InnerFuturePositionPtr long_pos = master.GetPosition(order.code, kBsFlagBuy, kOcFlagOpen);
        EXPECT_EQ(long_pos->td_open_volume_, 2);
        EXPECT_EQ(long_pos->GetYesterdayAvailableVolume(), 1);  // 此时还有一手昨仓
    }
}