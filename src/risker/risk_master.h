// Copyright 2021 Fancapital Inc.  All rights reserved.
#pragma once
#include <string>
#include <vector>
#include <memory>
#include "./risk_options.h"

namespace co {
    /**
     * 交易风控接口
     */
class RiskMaster {
 public:
    RiskMaster();
    ~RiskMaster();

    /**
      * 初始化
      * @param opts: 每个资金账号一个配置，data字段为JSON字符串，其具体内容由底层实现来定义；
      */
    void Init(const std::vector<std::shared_ptr<RiskOptions>>& opts);

    /**
     * 启动
     */
    void Start();

    /**
      * 处理委托请求，如果风控检查失败则返回错误信息，检查通过返回空字符串；
      * @params req: 委托请求消息
      * @return 错误信息
      */
    void HandleTradeOrderReq(MemTradeOrderMessage* req, std::string* error);

    /**
      * 处理委托响应
      * @params rep: 委托响应消息
      */
    void HandleTradeOrderRep(MemTradeOrderMessage* rep);

    /**
      * 处理撤单请求，如果风控检查失败则返回错误信息，检查通过返回空字符串；
      * @params req: 撤单请求消息
      * @return 错误信息
      */
    void HandleTradeWithdrawReq(MemTradeWithdrawMessage* req, std::string* error);

    /**
      * 处理撤单响应
      * @params rep: 撤单响应消息
      */
    void HandleTradeWithdrawRep(MemTradeWithdrawMessage* rep);

    /**
      * 处理成交回报
      * @params knock: 成交回报
      */
    void OnTradeKnock(MemTradeKnock* knock);

    /**
      * 处理行情，该函数仅用于测试；
      * @params tick: 行情数据
      */
    void OnTick(MemQTickBody* tick);

    /**
     * 获取内部队列长度
     * @return 队列长度
     */
    int64_t GetQueueSize() const;

 private:
    class RiskMasterImpl;
    RiskMasterImpl* m_ = nullptr;
};
}  // namespace co
