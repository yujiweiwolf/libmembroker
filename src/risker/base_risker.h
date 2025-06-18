// Copyright 2021 Fancapital Inc.  All rights reserved.
#pragma once
#include <string>
#include <vector>
#include <memory>
#include "risk_options.h"
#include "coral/coral.h"

#ifdef _WIN32
#define LOG_INFO __info
#define LOG_ERROR __error
#endif

namespace co {
/**
    * 交易风控接口，所有风控子类需继承并实现该接口；
    */
class Risker {
 public:
    /**
    * 初始化
    * @param opts: 每个资金账号一个配置，data字段为JSON字符串，其具体内容由底层实现来定义；
    */
    virtual void Init(std::shared_ptr<RiskOptions> opt);

    /**
    * 处理委托请求，该函数只进行风控检查，不进行任何状态更新；如果风控检查失败则返回错误信息，检查通过返回空字符串；
    * @params req: 委托请求消息
    * @return 错误信息
    */
    virtual std::string HandleTradeOrderReq(MemTradeOrderMessage* req);

    /**
     * 处理委托请求检查成功，在该函数内部进行状态更新；
     * @params req: 委托请求消息
     */
    virtual void OnTradeOrderReqPass(MemTradeOrderMessage* req);

    /**
    * 处理委托响应
    * @params rep: 委托响应消息
    */
    virtual void HandleTradeOrderRep(MemTradeOrderMessage* rep);

    /**
    * 处理撤单请求，该函数只进行风控检查，不进行任何状态更新；如果风控检查失败则返回错误信息，检查通过返回空字符串；
    * @params req: 撤单请求消息
    * @return 错误信息
    */
    virtual std::string HandleTradeWithdrawReq(MemTradeWithdrawMessage* req);

    /**
    * 处理撤单请求检查成功，在该函数内部进行状态更新；
    * @params req: 撤单请求消息
    * @return 错误信息
    */
    virtual void OnTradeWithdrawReqPass(MemTradeWithdrawMessage* req);

    /**
    * 处理撤单响应
    * @params rep: 撤单响应消息
    */
    virtual void HandleTradeWithdrawRep(MemTradeWithdrawMessage* rep);

    /**
    * 处理成交回报
    * @params knock: 成交回报
    */
    virtual void OnTradeKnock(MemTradeKnock* knock);
};
}  // namespace co


