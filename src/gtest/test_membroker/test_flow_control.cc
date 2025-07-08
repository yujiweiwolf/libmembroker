#include <string>
#include <gtest/gtest.h>

#include "../../mem_broker/flow_control.h"
#include "helper.h"

TEST(FlowControlTPSLimit, SizeOverflow) {
    //【测试目的】批量委托大小不能超过流控阈值
    //【测试参数】流控阈值：10，超时阈值：0-无超时
    //【测试输入】[1-批量买入（1个子委托），2-批量买入（10个子委托），3-批量买入（11个子委托），3-批量撤单（11个子委托）]
    //【测试步骤】依次发送请求，观察返回结果
    //【预期输出】[2-通过，3-废单, 4-撤单失败，1-通过]
    //【测试结果】
    std::string code = "510300.SH";
    co::BrokerQueue queue;
    co::FlowControlQueue fc(&queue);
    {
        auto cfg = std::make_unique<co::FlowControlConfig>();
        cfg->set_market(co::kMarketSH);
        cfg->set_th_tps_limit(10);
        co::MemBrokerOptionsPtr opt = std::make_shared<co::MemBrokerOptions>();
        opt->set_request_timeout_ms(5000);
        opt->mutable_flow_controls()->emplace_back(std::move(cfg));
        fc.Init(opt);
    }
    int64_t now = 20250618093000000;
    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateBatchOrder(1, "1",  "S1", now, code, co::kBsFlagBuy, 1.0, 100));
    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateBatchOrder(10, "2", "S1", now, code, co::kBsFlagBuy, 1.0, 100));
    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateBatchOrder(11, "3", "S1", now, code, co::kBsFlagBuy, 1.0, 100));
    queue.Push(nullptr, co::kMemTypeTradeWithdrawReq, FCCreateBatchWithdraw("4", "S1", now, "1-11-A"));
    std::vector<std::string> test_rows;
    while (true) {
        co::BrokerMsg* msg = fc.TryPop(x::AddRawDateTime(now, 0));
        if (!msg) {
            break;
        }
        if (msg->function_id() == co::kMemTypeTradeOrderReq) {
            co::MemTradeOrderMessage *req = (co::MemTradeOrderMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=ok");
        } else if (msg->function_id() == co::kMemTypeTradeOrderRep) {
            co::MemTradeOrderMessage *req = (co::MemTradeOrderMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=error");
        } else if (msg->function_id() == co::kMemTypeTradeWithdrawReq) {
            co::MemTradeWithdrawMessage *req = (co::MemTradeWithdrawMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=ok");
        } else if (msg->function_id() == co::kMemTypeTradeWithdrawRep) {
            co::MemTradeWithdrawMessage *req = (co::MemTradeWithdrawMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=error");
        } else {
            throw std::runtime_error("unknown function_id: " + std::to_string(msg->function_id()));
        }
    }
    while (true) {
        co::BrokerMsg* msg = fc.TryPop(x::AddRawDateTime(now, 10000));
        if (!msg) {
            break;
        }
        if (msg->function_id() == co::kMemTypeTradeOrderReq) {
            co::MemTradeOrderMessage *req = (co::MemTradeOrderMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=ok");
        } else if (msg->function_id() == co::kMemTypeTradeOrderRep) {
            co::MemTradeOrderMessage *req = (co::MemTradeOrderMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=error");
        } else if (msg->function_id() == co::kMemTypeTradeWithdrawReq) {
            co::MemTradeWithdrawMessage *req = (co::MemTradeWithdrawMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=ok");
        } else if (msg->function_id() == co::kMemTypeTradeWithdrawRep) {
            co::MemTradeWithdrawMessage *req = (co::MemTradeWithdrawMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=error");
        } else {
            throw std::runtime_error("unknown function_id: " + std::to_string(msg->function_id()));
        }
    }
    std::vector<std::string> ok_rows = {"2=ok", "3=error", "4=error", "1=ok"};
    std::string ok_line = x::ToString(ok_rows);
    std::string test_line = x::ToString(test_rows);
    if (ok_line != test_line) {
        std::cerr << "[  ok] " << ok_line << std::endl;
        std::cerr << "[test] " << test_line << std::endl;
    }
    ASSERT_EQ(test_line, ok_line);
}

TEST(FlowControlTPSLimit, Withdraw) {
    //【测试目的】撤单流控，批量撤单指令个数等于批量委托数量；
    //【测试参数】流控阈值：3，超时阈值：0-无超时
    //【测试输入】[1-单笔撤单，2-单笔撤单，3-批量撤单（3个子委托），4-批量撤单（4个子委托）]
    //【测试步骤】依次发送请求，观察返回结果
    //【预期输出】[1-通过，2-通过，4-废单，3-通过]
    //【测试结果】
    std::string code = "510300.SH";
    co::BrokerQueue queue;
    co::FlowControlQueue fc(&queue);
    {
        auto cfg = std::make_unique<co::FlowControlConfig>();
        cfg->set_market(co::kMarketSH);
        cfg->set_th_tps_limit(3);
        co::MemBrokerOptionsPtr opt = std::make_shared<co::MemBrokerOptions>();
        opt->set_request_timeout_ms(0);
        opt->mutable_flow_controls()->emplace_back(std::move(cfg));
        fc.Init(opt);
    }
    int64_t now = 20240730093000000;
    queue.Push(nullptr, co::kMemTypeTradeWithdrawReq, FCCreateWithdraw("1","S1", now, "1-A"));
    queue.Push(nullptr, co::kMemTypeTradeWithdrawReq, FCCreateWithdraw("2", "S1",now, "1-A"));
    queue.Push(nullptr, co::kMemTypeTradeWithdrawReq, FCCreateBatchWithdraw("3", "S1",now, "1-3-A"));
    queue.Push(nullptr, co::kMemTypeTradeWithdrawReq, FCCreateBatchWithdraw("4", "S1",now, "1-4-A"));
//    queue.Push(nullptr, co::kMemTypeTradeWithdrawReq, FCCreateWithdraw("5", now, ""));
    std::vector<std::string> test_rows;
    while (true) {
        co::BrokerMsg* msg = fc.TryPop(x::AddRawDateTime(now, 0));
        if (!msg) {
            break;
        }
        if (msg->function_id() == co::kMemTypeTradeOrderReq) {
            co::MemTradeOrderMessage *req = (co::MemTradeOrderMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=ok");
        } else if (msg->function_id() == co::kMemTypeTradeOrderRep) {
            co::MemTradeOrderMessage *req = (co::MemTradeOrderMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=error");
        } else if (msg->function_id() == co::kMemTypeTradeWithdrawReq) {
            co::MemTradeWithdrawMessage *req = (co::MemTradeWithdrawMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=ok");
        } else if (msg->function_id() == co::kMemTypeTradeWithdrawRep) {
            co::MemTradeWithdrawMessage *req = (co::MemTradeWithdrawMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=error");
        } else {
            throw std::runtime_error("unknown function_id: " + std::to_string(msg->function_id()));
        }
    }
    while (true) {
        co::BrokerMsg* msg = fc.TryPop(x::AddRawDateTime(now, 10000));
        if (!msg) {
            break;
        }
        if (msg->function_id() == co::kMemTypeTradeOrderReq) {
            co::MemTradeOrderMessage *req = (co::MemTradeOrderMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=ok");
        } else if (msg->function_id() == co::kMemTypeTradeOrderRep) {
            co::MemTradeOrderMessage *req = (co::MemTradeOrderMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=error");
        } else if (msg->function_id() == co::kMemTypeTradeWithdrawReq) {
            co::MemTradeWithdrawMessage *req = (co::MemTradeWithdrawMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=ok");
        } else if (msg->function_id() == co::kMemTypeTradeWithdrawRep) {
            co::MemTradeWithdrawMessage *req = (co::MemTradeWithdrawMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=error");
        } else {
            throw std::runtime_error("unknown function_id: " + std::to_string(msg->function_id()));
        }
    }
    std::vector<std::string> ok_rows = {"1=ok", "2=ok", "4=error", "3=ok"};
    std::string ok_line = x::ToString(ok_rows);
    std::string test_line = x::ToString(test_rows);
    if (ok_line != test_line) {
        std::cerr << "[  ok] " << ok_line << std::endl;
        std::cerr << "[test] " << test_line << std::endl;
    }
    ASSERT_EQ(test_line, ok_line);
}

TEST(FlowControlTPSLimit, MultiMarket) {
    //【测试目的】多个市场同时测试，每个市场单独进行流控
    //【测试参数】SH流控阈值：10，SZ流控阈值：5，超时阈值：0-无超时
    //【测试输入】[1-SH批量买入（1个子委托），2-SH批量买入（10个子委托），3-SH批量买入（1个子委托），4-SZ批量买入（1个子委托），5-SZ批量买入（10个子委托），6-SZ批量买入（1个子委托）]
    //【测试步骤】依次发送请求，观察返回结果
    //【预期输出】[2-SH通过，4-SZ通过，6-SZ通过，5=SZ废单，1-SH通过, 3-SH通过]
    //【测试结果】
    std::string sh_code = "510300.SH";
    std::string sz_code = "159901.SZ";
    co::BrokerQueue queue;
    co::FlowControlQueue fc(&queue);
    {
        co::MemBrokerOptionsPtr opt = std::make_shared<co::MemBrokerOptions>();
        opt->set_request_timeout_ms(0);
        {
            auto cfg = std::make_unique<co::FlowControlConfig>();
            cfg->set_market(co::kMarketSH);
            cfg->set_th_tps_limit(10);
            opt->mutable_flow_controls()->emplace_back(std::move(cfg));
        }
        {
            auto cfg = std::make_unique<co::FlowControlConfig>();
            cfg->set_market(co::kMarketSZ);
            cfg->set_th_tps_limit(5);
            opt->mutable_flow_controls()->emplace_back(std::move(cfg));
        }
        fc.Init(opt);
    }
    int64_t now = 20240730093000000;
    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateBatchOrder(1, "1", "S1", now, sh_code, co::kBsFlagBuy, 1.0, 100));
    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateBatchOrder(10, "2", "S1", now, sh_code, co::kBsFlagBuy, 1.0, 100));
    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateBatchOrder(1, "3", "S1", now, sh_code, co::kBsFlagBuy, 1.0, 100));
    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateBatchOrder(1, "4", "S1", now, sz_code, co::kBsFlagBuy, 1.0, 100));
    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateBatchOrder(10, "5", "S1", now, sz_code, co::kBsFlagBuy, 1.0, 100));
    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateBatchOrder(1, "6", "S1", now, sz_code, co::kBsFlagBuy, 1.0, 100));
    std::vector<std::string> test_rows;
    while (true) {
        co::BrokerMsg* msg = fc.TryPop(x::AddRawDateTime(now, 0));
        if (!msg) {
            break;
        }
        if (msg->function_id() == co::kMemTypeTradeOrderReq) {
            co::MemTradeOrderMessage *req = (co::MemTradeOrderMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=ok");
        } else if (msg->function_id() == co::kMemTypeTradeOrderRep) {
            co::MemTradeOrderMessage *req = (co::MemTradeOrderMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=error");
        } else if (msg->function_id() == co::kMemTypeTradeWithdrawReq) {
            co::MemTradeWithdrawMessage *req = (co::MemTradeWithdrawMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=ok");
        } else if (msg->function_id() == co::kMemTypeTradeWithdrawRep) {
            co::MemTradeWithdrawMessage *req = (co::MemTradeWithdrawMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=error");
        } else {
            throw std::runtime_error("unknown function_id: " + std::to_string(msg->function_id()));
        }
    }
    while (true) {
        co::BrokerMsg* msg = fc.TryPop(x::AddRawDateTime(now, 1550));
        if (!msg) {
            break;
        }
        if (msg->function_id() == co::kMemTypeTradeOrderReq) {
            co::MemTradeOrderMessage *req = (co::MemTradeOrderMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=ok");
        } else if (msg->function_id() == co::kMemTypeTradeOrderRep) {
            co::MemTradeOrderMessage *req = (co::MemTradeOrderMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=error");
        } else if (msg->function_id() == co::kMemTypeTradeWithdrawReq) {
            co::MemTradeWithdrawMessage *req = (co::MemTradeWithdrawMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=ok");
        } else if (msg->function_id() == co::kMemTypeTradeWithdrawRep) {
            co::MemTradeWithdrawMessage *req = (co::MemTradeWithdrawMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(string(req->id) + "=error");
        } else {
            throw std::runtime_error("unknown function_id: " + std::to_string(msg->function_id()));
        }
    }
    std::vector<std::string> ok_rows = {"2=ok", "4=ok", "6=ok", "5=error", "1=ok", "3=ok"};
    std::string ok_line = x::ToString(ok_rows);
    std::string test_line = x::ToString(test_rows);
    if (ok_line != test_line) {
        std::cerr << "[  ok] " << ok_line << std::endl;
        std::cerr << "[test] " << test_line << std::endl;
    }
    ASSERT_EQ(test_line, ok_line);
}

TEST(FlowControlTPSLimit, Priority) {
    //【测试目的】测试排序优先级
    //【测试参数】流控阈值：0-不限制，超时阈值：0-无超时
    //【测试输入】发送请求：[1-查询资金，2-查询持仓，3-买入100，4-买入100，5-买入200，6-卖出300，7-卖出400，8-申购，9-赎回，10-撤单]
    //【测试步骤】一次性将请求全部发送，观察返回结果，排序是否符合预期
    //【预期输出】[10-撤单，8-申购，9-赎回，7-卖出400，6-卖出300，5-买入200，3-买入100，4-买入100，1-查询资金，2-查询持仓]
    //【测试结果】
    std::string code = "510300.SH";
    co::BrokerQueue queue;
    co::FlowControlQueue fc(&queue);
    {
        auto cfg = std::make_unique<co::FlowControlConfig>();
        cfg->set_market(co::kMarketSH);
        cfg->set_th_tps_limit(0);
        co::MemBrokerOptionsPtr opt = std::make_shared<co::MemBrokerOptions>();
        opt->set_request_timeout_ms(0);
        opt->mutable_flow_controls()->emplace_back(std::move(cfg));
        fc.Init(opt);
    }
    int64_t now = 20240730093000000;
    queue.Push(nullptr, co::kMemTypeQueryTradeAssetReq, FCCreateQueryAsset("1", "S1", now));
    queue.Push(nullptr, co::kMemTypeQueryTradePositionReq, FCCreateQueryPosition("2", "S1", now));
    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("3", "S1", now, code, co::kBsFlagBuy, 1.0, 100));
    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("4", "S1", now, code, co::kBsFlagBuy, 1.0, 100));
    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("5", "S1", now, code, co::kBsFlagBuy, 1.0, 200));
    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("6", "S1", now, code, co::kBsFlagSell, 3.0, 100));
    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("7", "S1", now, code, co::kBsFlagSell, 1.0, 400));
    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("8", "S1", now, code, co::kBsFlagCreate, 0, 10000));
    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("9", "S1", now, code, co::kBsFlagRedeem, 0, 20000));
    queue.Push(nullptr, co::kMemTypeTradeWithdrawReq, FCCreateWithdraw("10", "S1", now, "1-A"));
    std::vector<int64_t> test_rows;
    while (true) {
        co::BrokerMsg* msg = fc.TryPop();
        if (!msg) {
            break;
        }
        if (msg->function_id() == co::kMemTypeTradeOrderReq) {
            co::MemTradeOrderMessage *req = (co::MemTradeOrderMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(x::ToInt64(req->id));
        } else if (msg->function_id() == co::kMemTypeTradeOrderRep) {
            co::MemTradeOrderMessage *req = (co::MemTradeOrderMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(x::ToInt64(req->id));
        } else if (msg->function_id() == co::kMemTypeTradeWithdrawReq) {
            co::MemTradeWithdrawMessage *req = (co::MemTradeWithdrawMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(x::ToInt64(req->id));
        } else if (msg->function_id() == co::kMemTypeTradeWithdrawRep) {
            co::MemTradeWithdrawMessage *req = (co::MemTradeWithdrawMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(x::ToInt64(req->id));
        } else if (msg->function_id() == co::kMemTypeQueryTradeAssetReq) {
            co::MemGetTradeAssetMessage *req = (co::MemGetTradeAssetMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(x::ToInt64(req->id));
        } else if (msg->function_id() == co::kMemTypeQueryTradePositionReq) {
            co::MemGetTradePositionMessage *req = (co::MemGetTradePositionMessage *)(reinterpret_cast<const void*>(msg->data().data()));
            test_rows.emplace_back(x::ToInt64(req->id));
        } else {
            throw std::runtime_error("unknown function_id: " + std::to_string(msg->function_id()));
        }
    }
    std::vector<int64_t> ok_rows = {10, 8, 9, 7, 6, 5, 3, 4, 1, 2};
    std::string ok_line = x::ToString(ok_rows);
    std::string test_line = x::ToString(test_rows);
    if (ok_line != test_line) {
        std::cerr << "[  ok] " << ok_line << std::endl;
        std::cerr << "[test] " << test_line << std::endl;
    }
    ASSERT_EQ(test_line, ok_line);
}

//TEST(FlowControlTPSLimit, Speed) {
//    //【测试目的】测试流控速度
//    //【测试参数】流控阈值：2-每秒2笔报撤单，超时阈值：0-无超时
//    //【测试输入】分两批次发送请求，优先级全部相同：[1,2,3,4], [5,6]
//    //【测试步骤】[0000ms] 发送第一批次的请求1-4，观察返回结果
//    //          [1000ms] 刚好1秒钟的时刻，观察返回结果
//    //          [1001ms] 时间过去10001，观察返回结果；然后发送第二批请求
//    //          [2000ms] 观察返回结果
//    //          [2001ms] 观察返回结果
//    //          [2002ms] 观察返回结果
//    //【预期输出】[0000ms] 发送第一批次的请求后，立即返回1和2，其他请求继续排队等待；
//    //          [1000ms] 无返回，继续等待
//    //          [1001ms] 返回3和4
//    //          [2000ms] 无返回，继续等待
//    //          [2001ms] 无返回，继续等待
//    //          [2002ms] 返回5和6
//    //【测试结果】
//    std::string code = "510300.SH";
//    co::BrokerQueue queue;
//    co::FlowControlQueue fc(&queue);
//    {
//        auto cfg = std::make_unique<co::FlowControlConfig>();
//        cfg->set_market(co::kMarketSH);
//        cfg->set_th_tps_limit(2);  // 每秒只能报2个指令
//        co::BrokerOptions opt;
//        opt.set_request_timeout_ms(0);
//        opt.mutable_flow_controls()->emplace_back(std::move(cfg));
//        fc.Init(opt);
//    }
//    int64_t now = 20240730093000000;
//    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("1", now, code, co::kBsFlagBuy, 1.0, 100));
//    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("2", now, code, co::kBsFlagBuy, 1.0, 100));
//    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("3", now, code, co::kBsFlagBuy, 1.0, 100));
//    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("4", now, code, co::kBsFlagBuy, 1.0, 100));
//    ASSERT_NE(fc.TryPop(now), nullptr); // 0000ms 报单1
//    ASSERT_NE(fc.TryPop(now), nullptr); // 0000ms 报单2
//    ASSERT_EQ(fc.TryPop(now), nullptr);
//    ASSERT_EQ(fc.TryPop(x::AddRawDateTime(now, co::kFlowControlWindowMS + 0)), nullptr);
//    ASSERT_NE(fc.TryPop(x::AddRawDateTime(now, co::kFlowControlWindowMS + 1)), nullptr); // 1001ms 报单3
//    ASSERT_NE(fc.TryPop(x::AddRawDateTime(now, co::kFlowControlWindowMS + 1)), nullptr); // 1001ms 报单4
//    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("5", now, code, co::kBsFlagBuy, 1.0, 100));
//    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("6", now, code, co::kBsFlagBuy, 1.0, 100));
//    ASSERT_EQ(fc.TryPop(x::AddRawDateTime(now, co::kFlowControlWindowMS * 2)), nullptr);
//    ASSERT_EQ(fc.TryPop(x::AddRawDateTime(now, co::kFlowControlWindowMS * 2 + 1)), nullptr);
//    ASSERT_NE(fc.TryPop(x::AddRawDateTime(now, co::kFlowControlWindowMS * 2 + 2)), nullptr); // 2002ms 报单5
//    ASSERT_NE(fc.TryPop(x::AddRawDateTime(now, co::kFlowControlWindowMS * 2 + 2)), nullptr); // 2002ms 报单6
//}
//
//TEST(FlowControlTPSLimit, RequestTimeout) {
//    //【测试目的】检查超时逻辑是否正确，检查"撤单永远不会超时"是否正确；
//    //【测试参数】流控阈值：0-不限制，超时阈值：5000-5秒超时
//    //【测试输入】发送请求：[1-已延迟6s的买入，2-已延迟1秒的买入，3-已延迟6秒的撤单]
//    //【测试步骤】一次性将请求全部发送，观察返回结果，排序是否符合预期
//    //【预期输出】[3-已延迟6秒的撤单(未超时)，1-已延迟6s的买入(超时废单)，2-已延迟1秒的买入(未超时)]
//    //【测试结果】
//    std::string code = "510300.SH";
//    co::BrokerQueue queue;
//    co::FlowControlQueue fc(&queue);
//    {
//        auto cfg = std::make_unique<co::FlowControlConfig>();
//        cfg->set_market(co::kMarketSH);
//        cfg->set_th_tps_limit(0);
//        co::BrokerOptions opt;
//        opt.set_request_timeout_ms(5000);
//        opt.mutable_flow_controls()->emplace_back(std::move(cfg));
//        fc.Init(opt);
//    }
//    int64_t now = 20240730093000000;
//    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("1", x::AddRawDateTime(now, -6000), code, co::kBsFlagBuy, 1.0, 100));
//    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("2", x::AddRawDateTime(now, -1000), code, co::kBsFlagBuy, 1.0, 100));
//    queue.Push(nullptr, co::kMemTypeTradeWithdrawReq, FCCreateWithdraw("3", x::AddRawDateTime(now, -6000), "1-A"));
//    std::vector<std::string> test_rows;
//    while (true) {
//        co::BrokerMsg* msg = fc.TryPop(now);
//        if (!msg) {
//            break;
//        }
//        if (msg->function_id() == co::kMemTypeTradeOrderReq) {
//            auto req = flatbuffers::GetRoot<co::fbs::TradeOrderMessage>(msg->data().data());
//            test_rows.emplace_back(req->id()->str() + "=ok");
//        } else if (msg->function_id() == co::kMemTypeTradeOrderRep) {
//            auto rep = flatbuffers::GetRoot<co::fbs::TradeOrderMessage>(msg->data().data());
//            test_rows.emplace_back(rep->id()->str() + "=timeout");
//        } else if (msg->function_id() == co::kMemTypeTradeWithdrawReq) {
//            auto req = flatbuffers::GetRoot<co::fbs::TradeWithdrawMessage>(msg->data().data());
//            test_rows.emplace_back(req->id()->str() + "=ok");
//        } else if (msg->function_id() == co::kMemTypeTradeWithdrawRep) {
//            auto req = flatbuffers::GetRoot<co::fbs::TradeWithdrawMessage>(msg->data().data());
//            test_rows.emplace_back(req->id()->str() + "=timeout");
//        } else {
//            throw std::runtime_error("unknown function_id: " + std::to_string(msg->function_id()));
//        }
//    }
//    std::vector<std::string> ok_rows = {"3=ok", "1=timeout", "2=ok"};
//    std::string ok_line = x::ToString(ok_rows);
//    std::string test_line = x::ToString(test_rows);
//    if (ok_line != test_line) {
//        std::cerr << "[  ok] " << ok_line << std::endl;
//        std::cerr << "[test] " << test_line << std::endl;
//    }
//    ASSERT_EQ(test_line, ok_line);
//}
//
//TEST(FlowControlTPSLimit, FlowControlTimeout) {
//    //【测试目的】流控超时，检查因排队导致的超时处理是否正确；低优先级的本来不超时，高优先级的挤掉低优先级的，导致低优先级的出现超时。
//    //【测试参数】流控阈值：2-每秒2笔报撤单，超时阈值：2000-2秒超时
//    //【测试输入】分两批次发送请求：[1-买入，2-买入，3-买入，4-买入，5-买入，6-买入], [7-买入，8-申购]
//    //【测试步骤】[0000ms] 第一批，发送1-6的消息，观察返回结果；
//    //          [0000ms] 第二批，继续发送7-8两个消息，观察返回结果；
//    //          [1001ms] 观察返回结果
//    //          [2002ms] 观察返回结果
//    //【预期输出】[0000ms] 第一批发送后，立即返回1和2，其他继续等待
//    //          [0000ms] 第二批发送7后，立即返回7的报单超时废单响应；
//    //          [0000ms] 第二批发送8后，立即返回6的报单超时废单响应；因为8的优先级更高，会将优先级最低而且接收时间最晚的6挤成超时废单；
//    //          [1001ms] 在系统时间大于1秒钟之后，立即返回8和3；
//    //          [2002ms] 在系统时间大于2秒钟之后，此时4和5已经等待超时，立即返回4和5的超时废单响应；
//    //【测试结果】
//    std::string code = "510300.SH";
//    co::BrokerQueue queue;
//    co::FlowControlQueue fc(&queue);
//    {
//        auto cfg = std::make_unique<co::FlowControlConfig>();
//        cfg->set_market(co::kMarketSH);
//        cfg->set_th_tps_limit(2);  // 每秒只允许2个指令
//        co::BrokerOptions opt;
//        opt.set_request_timeout_ms(2000);  // 排队超过2秒会超时
//        opt.mutable_flow_controls()->emplace_back(std::move(cfg));
//        fc.Init(opt);
//    }
//    int64_t now = 20240730093000000;
//    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("1", x::AddRawDateTime(now, 0), code, co::kBsFlagBuy, 1.0, 100));
//    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("2", x::AddRawDateTime(now, 0), code, co::kBsFlagBuy, 1.0, 100));
//    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("3", x::AddRawDateTime(now, 0), code, co::kBsFlagBuy, 1.0, 100));
//    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("4", x::AddRawDateTime(now, 0), code, co::kBsFlagBuy, 1.0, 100));
//    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("5", x::AddRawDateTime(now, 0), code, co::kBsFlagBuy, 1.0, 100));
//    queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("6", x::AddRawDateTime(now, 0), code, co::kBsFlagBuy, 1.0, 100));
//    ASSERT_NE(fc.TryPop(now), nullptr);  // 【0000ms】弹出：1
//    ASSERT_NE(fc.TryPop(now), nullptr);  // 【0000ms】弹出：2
//    ASSERT_EQ(fc.TryPop(now), nullptr);  // 【0000ms】等待：3,4,5,6
//    {  //【0000ms】发送7，预计要等3秒才能报单，立即变为超时废单
//        queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("7", x::AddRawDateTime(now, 0), code, co::kBsFlagBuy, 1.0, 100));
//        co::BrokerMsg* msg = fc.TryPop(now);
//        ASSERT_NE(msg, nullptr);
//        ASSERT_EQ(msg->function_id(), co::kMemTypeTradeOrderRep);
//        auto rep = flatbuffers::GetRoot<co::fbs::TradeOrderMessage>(msg->data().data());
//        std::string id = rep->id() ? rep->id()->str() : "";
//        ASSERT_EQ(id, "7");
//        std::string error = rep->error() ? rep->error()->str() : "";
//        ASSERT_TRUE(!error.empty());
//    }
//    {  //【0000ms】发送8，发送更高优先级的报单，会将6挤成超时废单
//        queue.Push(nullptr, co::kMemTypeTradeOrderReq, FCCreateOrder("8", x::AddRawDateTime(now, 0), code, co::kBsFlagCreate, 0, 10000));
//        co::BrokerMsg* msg = fc.TryPop(now);
//        ASSERT_NE(msg, nullptr);
//        ASSERT_EQ(msg->function_id(), co::kMemTypeTradeOrderRep);
//        auto rep = flatbuffers::GetRoot<co::fbs::TradeOrderMessage>(msg->data().data());
//        std::string id = rep->id() ? rep->id()->str() : "";
//        ASSERT_EQ(id, "6");
//        std::string error = rep->error() ? rep->error()->str() : "";
//        ASSERT_TRUE(!error.empty());
//    }
//    {  //【1001ms】1秒钟之后，弹出8和3
//        co::BrokerMsg* msg = fc.TryPop(x::AddRawDateTime(now, co::kFlowControlWindowMS + 1));
//        ASSERT_NE(msg, nullptr);
//        ASSERT_EQ(msg->function_id(), co::kMemTypeTradeOrderReq);
//        auto req = flatbuffers::GetRoot<co::fbs::TradeOrderMessage>(msg->data().data());
//        std::string id = req->id() ? req->id()->str() : "";
//        ASSERT_EQ(id, "8");
//        std::string error = req->error() ? req->error()->str() : "";
//        ASSERT_TRUE(error.empty());
//    }
//    {  //【1001ms】1秒钟之后，弹出8和3
//        co::BrokerMsg* msg = fc.TryPop(x::AddRawDateTime(now, co::kFlowControlWindowMS + 1));
//        ASSERT_NE(msg, nullptr);
//        ASSERT_EQ(msg->function_id(), co::kMemTypeTradeOrderReq);
//        auto req = flatbuffers::GetRoot<co::fbs::TradeOrderMessage>(msg->data().data());
//        std::string id = req->id() ? req->id()->str() : "";
//        ASSERT_EQ(id, "3");
//        std::string error = req->error() ? req->error()->str() : "";
//        ASSERT_TRUE(error.empty());
//    }
//    {  //【2002ms】2秒钟之后，4和5已经超时
//        co::BrokerMsg* msg = fc.TryPop(x::AddRawDateTime(now, co::kFlowControlWindowMS * 2 + 2));
//        ASSERT_NE(msg, nullptr);
//        ASSERT_EQ(msg->function_id(), co::kMemTypeTradeOrderRep);
//        auto rep = flatbuffers::GetRoot<co::fbs::TradeOrderMessage>(msg->data().data());
//        std::string id = rep->id() ? rep->id()->str() : "";
//        ASSERT_EQ(id, "4");
//        std::string error = rep->error() ? rep->error()->str() : "";
//        ASSERT_TRUE(!error.empty());
//    }
//    {  // 【2002ms】2秒钟之后，4和5已经超时
//        co::BrokerMsg* msg = fc.TryPop(x::AddRawDateTime(now, co::kFlowControlWindowMS * 2 + 2));
//        ASSERT_NE(msg, nullptr);
//        ASSERT_EQ(msg->function_id(), co::kMemTypeTradeOrderRep);
//        auto rep = flatbuffers::GetRoot<co::fbs::TradeOrderMessage>(msg->data().data());
//        std::string id = rep->id() ? rep->id()->str() : "";
//        ASSERT_EQ(id, "5");
//        std::string error = rep->error() ? rep->error()->str() : "";
//        ASSERT_TRUE(!error.empty());
//    }
//}
