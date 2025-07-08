#include <gtest/gtest.h>
#include "yaml-cpp/yaml.h"

#include "../../risker/risk_master.h"
#include "../../risker/risk_options.h"
#include "../../risker/common/order_book.h"
using namespace co;

std::string fund_id = "S1";
bool init_flag = false;
std::shared_ptr<RiskMaster> risk;
// 注意事项，每个测试案例中的code须不一样

void GenerateRiskOptions(std::vector<std::shared_ptr<RiskOptions>>& opts) {
    std::shared_ptr<RiskOptions> _opt_1 = std::make_shared<RiskOptions>();
    _opt_1->set_id("a0ca4737da0047f0bb69d909b9d9a7ae");
    _opt_1->set_risker_id("zhongxin");
    _opt_1->set_fund_id(fund_id);
    _opt_1->set_disabled(false);
    std::string data = "{\"TYPE\":8,\"enable_prevent_self_knock\":true,\"only_etf_anti_self_knock\":false,\"white_list\":\"192.168.0.1,18:26:49:3E:A6:51\",\"name\":\"普通测试帐号\"}";
    _opt_1->set_data(data);
    opts.push_back(_opt_1);
}

void GenerateTradeOrderMessage(MemTradeOrderMessage* msg, const string& code, double price, int64_t bs_flag, int64_t volume, int64_t oc_flag) {
    string id = x::UUID();
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id.c_str());
    msg->timestamp = x::RawDateTime();
    msg->bs_flag = bs_flag;
    msg->items_size = 1;
    MemTradeOrder* item = (MemTradeOrder*)((char*)msg + sizeof(MemTradeOrderMessage));
    for (int i = 0; i < 1; i++) {
        MemTradeOrder* order = item + i;
        order->volume = volume;
        order->price = price;
        order->price_type = kQOrderTypeLimit;
        strcpy(order->code, code.c_str());
        LOG_INFO << "生成报单参数, code: " << order->code << ", volume: " << order->volume
                 << ", price: " << order->price << ", bs_flag: " << bs_flag;
    }
}

void Init() {
    if (!init_flag) {
        init_flag = true;
        risk = std::make_shared<RiskMaster>();
        std::vector<std::shared_ptr<RiskOptions>> opts;
        GenerateRiskOptions(opts);
        risk->Init(opts);
        risk->Start();
        x::Sleep(1000);
    }
}

/*
【测试目的】存在买单, 再挂卖单会出错
【测试步骤】1. 报买单
          2. 报卖单, 价格高于买单, 报单成功
          3. 报卖单, 价格等于买单, 报单失败
          4. 给买单报单回报和成交回报
          5. 再报卖单, 价格等于买单, 报单成功
*/

TEST(Risker, Buy) {
    Init();

    std::string code = "600000.SH";
    double order_price = 9.98;
    string order_no = "1-0";
    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
    char buffer[length] = "";
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
    GenerateTradeOrderMessage(msg, code, order_price, kBsFlagBuy, 200, kOcFlagOpen);
    std::string out;
    risk->HandleTradeOrderReq(msg, &out);
    LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
    EXPECT_STREQ(out.c_str(), "");
    MemTradeOrder* items = msg->items;
    MemTradeOrder* order = items + 0;
    strcpy(order->order_no, order_no.c_str());
    risk->HandleTradeOrderRep(msg);

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code, order_price + 0.01, kBsFlagSell, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(out.empty());
    }

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code, order_price, kBsFlagSell, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(!out.empty());
    }

    {
        MemTradeKnock knock = {};
        knock.timestamp = x::RawDateTime();
        strcpy(knock.fund_id, msg->fund_id);
        strcpy(knock.code, order->code);
        strcpy(knock.order_no, order->order_no);
        knock.bs_flag = msg->bs_flag;
        knock.match_volume = order->volume;
        knock.match_price = order->price;
        knock.match_type = co::kMatchTypeOK;
        risk->OnTradeKnock(&knock);
    }

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code, order_price, kBsFlagSell, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(out.empty());
    }
}

TEST(Risker, Sell) {
    Init();

    std::string code = "600001.SH";
    double order_price = 9.98;
    string order_no = "1-1";
    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
    char buffer[length] = "";
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
    GenerateTradeOrderMessage(msg, code, order_price, kBsFlagSell, 200, kOcFlagOpen);
    std::string out;
    risk->HandleTradeOrderReq(msg, &out);
    LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
    EXPECT_STREQ(out.c_str(), "");
    MemTradeOrder* items = msg->items;
    MemTradeOrder* order = items + 0;
    strcpy(order->order_no, order_no.c_str());
    risk->HandleTradeOrderRep(msg);

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code, order_price - 0.01, kBsFlagBuy, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(out.empty());
    }

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code, order_price, kBsFlagBuy, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(!out.empty());
    }

    {
        MemTradeKnock knock = {};
        knock.timestamp = x::RawDateTime();
        strcpy(knock.fund_id, msg->fund_id);
        strcpy(knock.code, order->code);
        strcpy(knock.order_no, order->order_no);
        knock.bs_flag = msg->bs_flag;
        knock.match_volume = order->volume;
        knock.match_price = order->price;
        knock.match_type = co::kMatchTypeWithdrawOK;
        risk->OnTradeKnock(&knock);
    }

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code, order_price, kBsFlagBuy, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(out.empty());
    }
}

TEST(Risker, OrderTimeOut) {
    Init();

    std::string code = "600003.SH";
    double order_price = 9.98;
    string order_no = "1-3";
    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
    char buffer[length] = "";
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
    GenerateTradeOrderMessage(msg, code, order_price, kBsFlagBuy, 200, kOcFlagOpen);
    std::string out;
    risk->HandleTradeOrderReq(msg, &out);
    LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
    EXPECT_STREQ(out.c_str(), "");

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code, order_price, kBsFlagSell, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(!out.empty());

        x::Sleep(co::kOrderTimeoutMS);

        out.clear();
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(out.empty());
    }
}

TEST(Risker, WithdrawSuccess) {
    Init();

    std::string code = "600004.SH";
    double order_price = 9.98;
    string order_no = "1-4";
    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
    char buffer[length] = "";
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
    GenerateTradeOrderMessage(msg, code, order_price, kBsFlagBuy, 200, kOcFlagOpen);
    std::string out;
    risk->HandleTradeOrderReq(msg, &out);
    LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
    EXPECT_STREQ(out.c_str(), "");
    MemTradeOrder* items = msg->items;
    MemTradeOrder* order = items + 0;
    strcpy(order->order_no, order_no.c_str());
    risk->HandleTradeOrderRep(msg);

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code, order_price, kBsFlagSell, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(!out.empty());
    }

    // 撤单成功
    {
        MemTradeWithdrawMessage req = {};
        string id = x::UUID();
        strncpy(req.id, id.c_str(), id.length());
        strcpy(req.fund_id, fund_id.c_str());
        strcpy(req.order_no, order_no.c_str());
        req.timestamp = x::RawDateTime();
        risk->HandleTradeWithdrawRep(&req);
    }

    // 再次报单
    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code, order_price, kBsFlagSell, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(out.empty());
    }
}


TEST(Risker, WithdrawFailed) {
    Init();

    std::string code = "600005.SH";
    double order_price = 9.98;
    string order_no = "1-5";
    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
    char buffer[length] = "";
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
    GenerateTradeOrderMessage(msg, code, order_price, kBsFlagBuy, 200, kOcFlagOpen);
    std::string out;
    risk->HandleTradeOrderReq(msg, &out);
    LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
    EXPECT_STREQ(out.c_str(), "");
    MemTradeOrder* items = msg->items;
    MemTradeOrder* order = items + 0;
    strcpy(order->order_no, order_no.c_str());
    risk->HandleTradeOrderRep(msg);

    // 撤单失败
    {
        MemTradeWithdrawMessage req = {};
        string id = x::UUID();
        strncpy(req.id, id.c_str(), id.length());
        strcpy(req.fund_id, fund_id.c_str());
        strcpy(req.order_no, order_no.c_str());
        req.timestamp = x::RawDateTime();
        strcpy(req.error, "撤单失败");
        risk->HandleTradeWithdrawRep(&req);
    }

    // 再次报单
    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code, order_price, kBsFlagSell, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(!out.empty());

        x::Sleep(co::kOrderTimeoutMS + 1000);
        {
            std::string out;
            risk->HandleTradeOrderReq(msg, &out);
            LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
            EXPECT_TRUE(out.empty());
        }
    }
}

//【测试步骤】
//1. 报批量买单，第1、3、5笔成功， 第2、4笔失败
//2. 报第一笔中的code对应的卖单，失败
//3. 报第一笔中的code对应的卖单，成功
//4. 给成交回报
//5. 报批量卖单，对买单一一对应，成功

TEST(Risker, BatchOrderBuy) {
    Init();

    string code_0, code_1;
    double order_price = 8.88;
    int total_num = 5;
    string batch_no = "1-5-4243442144";
    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_num;
    char buffer[length] = "";
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
    string id = x::UUID();
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id.c_str());
    msg->timestamp = x::RawDateTime();
    msg->bs_flag = co::kBsFlagBuy;
    msg->items_size = total_num;
    MemTradeOrder* item = (MemTradeOrder*)((char*)msg + sizeof(MemTradeOrderMessage));
    for (int i = 0; i < total_num; i++) {
        MemTradeOrder* order = item + i;
        order->volume = 100 + i;
        order->price = order_price;
        order->price_type = kQOrderTypeLimit;
        sprintf(order->code, "50100%d.SH", i);
        if (i == 0) {
            code_0 = order->code;
        } else if (i == 1) {
            code_1 = order->code;
        }
        LOG_INFO << "生成报单参数, code: " << order->code << ", volume: " << order->volume
                 << ", price: " << order->price << ", bs_flag: " << msg->bs_flag;
    }

    std::string out;
    risk->HandleTradeOrderReq(msg, &out);
    LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
    EXPECT_STREQ(out.c_str(), "");
    MemTradeOrder* items = msg->items;
    for (int i = 0; i < total_num; i++) {
        MemTradeOrder* order = items + i;
        if (i % 2 == 0) {
            sprintf(order->order_no, "2-%d", i);
        } else {
            strcpy(order->error, "报单失败");
        }
    }
    strcpy(msg->batch_no, batch_no.c_str());
    risk->HandleTradeOrderRep(msg);

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code_0, order_price, kBsFlagSell, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_FALSE(out.empty());
    }

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code_1, order_price, kBsFlagSell, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(out.empty());
    }

    for (int i = 0; i < total_num; i++) {
        MemTradeOrder* order = items + i;
        if (strlen(order->order_no) > 0) {
            MemTradeKnock knock = {};
            knock.timestamp = x::RawDateTime();
            strcpy(knock.fund_id, msg->fund_id);
            strcpy(knock.code, order->code);
            strcpy(knock.order_no, order->order_no);
            strcpy(knock.batch_no, msg->batch_no);
            knock.bs_flag = msg->bs_flag;
            knock.match_volume = order->volume;
            knock.match_price = order->price;
            knock.match_type = co::kMatchTypeOK;
            risk->OnTradeKnock(&knock);
        }
    }

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code_0, order_price, kBsFlagSell, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(out.empty());
    }

    {
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        string id = x::UUID();
        strncpy(msg->id, id.c_str(), id.length());
        strcpy(msg->fund_id, fund_id.c_str());
        msg->timestamp = x::RawDateTime();
        msg->bs_flag = co::kBsFlagSell;
        msg->items_size = total_num;
        MemTradeOrder* item = (MemTradeOrder*)((char*)msg + sizeof(MemTradeOrderMessage));
        for (int i = 0; i < total_num; i++) {
            MemTradeOrder* order = item + i;
            order->volume = 100 + i;
            order->price = order_price;
            order->price_type = kQOrderTypeLimit;
            sprintf(order->code, "50100%d.SH", i);
            if (i == 0) {
                code_0 = order->code;
            } else if (i == 1) {
                code_1 = order->code;
            }
            LOG_INFO << "生成报单参数, code: " << order->code << ", volume: " << order->volume
                     << ", price: " << order->price << ", bs_flag: " << msg->bs_flag;
        }
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_STREQ(out.c_str(), "");
    }
}

TEST(Risker, BatchWithdraw) {
    Init();

    string code_0, code_1;
    double order_price = 7.77;
    int total_num = 5;
    string batch_no = "1-5-77664443";
    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_num;
    char buffer[length] = "";
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
    string id = x::UUID();
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id.c_str());
    msg->timestamp = x::RawDateTime();
    msg->bs_flag = co::kBsFlagSell;
    msg->items_size = total_num;
    MemTradeOrder* item = (MemTradeOrder*)((char*)msg + sizeof(MemTradeOrderMessage));
    for (int i = 0; i < total_num; i++) {
        MemTradeOrder* order = item + i;
        order->volume = 100 + i;
        order->price = order_price;
        order->price_type = kQOrderTypeLimit;
        sprintf(order->code, "50200%d.SH", i);
        if (i == 0) {
            code_0 = order->code;
        } else if (i == 1) {
            code_1 = order->code;
        }
        LOG_INFO << "生成报单参数, code: " << order->code << ", volume: " << order->volume
                 << ", price: " << order->price << ", bs_flag: " << msg->bs_flag;
    }

    std::string out;
    risk->HandleTradeOrderReq(msg, &out);
    LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
    EXPECT_STREQ(out.c_str(), "");
    MemTradeOrder* items = msg->items;
    for (int i = 0; i < total_num; i++) {
        MemTradeOrder* order = items + i;
        if (i % 2 == 0) {
            sprintf(order->order_no, "2-%d", i);
        } else {
            strcpy(order->error, "报单失败");
        }
    }
    strcpy(msg->batch_no, batch_no.c_str());
    risk->HandleTradeOrderRep(msg);

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code_0, order_price, kBsFlagBuy, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_FALSE(out.empty());
    }

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code_1, order_price, kBsFlagBuy, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(out.empty());
    }

    {
        MemTradeWithdrawMessage req = {};
        string id = x::UUID();
        strncpy(req.id, id.c_str(), id.length());
        strcpy(req.fund_id, fund_id.c_str());
        strcpy(req.batch_no, batch_no.c_str());
        req.timestamp = x::RawDateTime();
        risk->HandleTradeWithdrawRep(&req);
    }

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code_0, order_price, kBsFlagBuy, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(out.empty());
    }

    // 批量买单
    {
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        string id = x::UUID();
        strncpy(msg->id, id.c_str(), id.length());
        strcpy(msg->fund_id, fund_id.c_str());
        msg->timestamp = x::RawDateTime();
        msg->bs_flag = co::kBsFlagBuy;
        msg->items_size = total_num;
        MemTradeOrder* item = (MemTradeOrder*)((char*)msg + sizeof(MemTradeOrderMessage));
        for (int i = 0; i < total_num; i++) {
            MemTradeOrder* order = item + i;
            order->volume = 100 + i;
            order->price = order_price;
            order->price_type = kQOrderTypeLimit;
            sprintf(order->code, "50100%d.SH", i);
            if (i == 0) {
                code_0 = order->code;
            } else if (i == 1) {
                code_1 = order->code;
            }
            LOG_INFO << "生成报单参数, code: " << order->code << ", volume: " << order->volume
                     << ", price: " << order->price << ", bs_flag: " << msg->bs_flag;
        }
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_STREQ(out.c_str(), "");
    }
}

// 其它broker的单子，只有报单响应和成交
TEST(Risker, OtherOrder) {
    Init();

    std::string code = "503001.SH";
    double order_price = 9.11;
    string order_no = "1-1";
    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
    char buffer[length] = "";
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
    GenerateTradeOrderMessage(msg, code, order_price, kBsFlagBuy, 200, kOcFlagOpen);
    strcpy(msg->fund_id, "ADHHFHFHAGGADHD");
    MemTradeOrder* items = msg->items;
    MemTradeOrder* order = items + 0;
    strcpy(order->order_no, order_no.c_str());
    risk->HandleTradeOrderRep(msg);

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code, order_price, kBsFlagSell, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_FALSE(out.empty());
    }

    {
        MemTradeKnock knock = {};
        knock.timestamp = x::RawDateTime();
        strcpy(knock.fund_id, msg->fund_id);
        strcpy(knock.code, order->code);
        strcpy(knock.order_no, order->order_no);
        knock.bs_flag = msg->bs_flag;
        knock.match_volume = order->volume;
        knock.match_price = order->price;
        knock.match_type = co::kMatchTypeWithdrawOK;
        risk->OnTradeKnock(&knock);
    }

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code, order_price, kBsFlagSell, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(out.empty());
    }
}

TEST(Risker, OnTick) {
    Init();

    std::string code = "504001.SH";
    double order_price = 11.11;
    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
    char buffer[length] = "";
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
    GenerateTradeOrderMessage(msg, code, order_price, kBsFlagBuy, 200, kOcFlagOpen);
    std::string out;
    risk->HandleTradeOrderReq(msg, &out);
    LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code, order_price, kBsFlagSell, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_FALSE(out.empty());
    }

    MemQTickBody tick = {};
    strcpy(tick.code, code.c_str());
    tick.timestamp = msg->timestamp + 10000 + 1000;
    tick.ap[0] = order_price;
    tick.av[0] = 1;
    tick.bp[0] = order_price - 0.01;
    tick.bv[0] = 1;
    tick.new_price = order_price;
    risk->OnTick(&tick);

    {
        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder);
        char buffer[length] = "";
        MemTradeOrderMessage* msg = (MemTradeOrderMessage*)buffer;
        GenerateTradeOrderMessage(msg, code, order_price, kBsFlagSell, 100, kOcFlagOpen);
        std::string out;
        risk->HandleTradeOrderReq(msg, &out);
        LOG_INFO << "报单结果: " << (out.empty() ? "成功" : out);
        EXPECT_TRUE(out.empty());
    }
}

TEST(Risker, Wait) {
    x::Sleep(10000);
}


