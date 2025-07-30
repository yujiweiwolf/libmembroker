// Copyright 2025 Fancapital Inc.  All rights reserved.
#pragma once
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include "../base_risker.h"

namespace co {

    /**
     * 平凡投资风控
     */
    class FancapitalRisker : public Risker {
    public:
        /**
            * 初始化
            * @param opts: 每个资金账号一个配置，data字段为JSON字符串，其具体内容由底层实现来定义；
            */
        virtual void Init(std::shared_ptr<RiskOptions> opt);
//
//        /**
//            * 处理委托请求，如果风控检查失败则返回错误信息，检查通过返回空字符串；
//            * @params req: 委托请求消息
//            * @return 错误信息
//            */
//        virtual void HandleTradeOrderReq(MemTradeOrderMessage* req, std::string* error);
//
//        /**
//            * 处理委托响应
//            * @params rep: 委托响应消息
//            */
//        virtual void HandleTradeOrderRep(const co::fbs::TradeOrderMessage* rep);
//
//        /**
//            * 处理撤单请求，如果风控检查失败则返回错误信息，检查通过返回空字符串；
//            * @params req: 撤单请求消息
//            * @return 错误信息
//            */
//        virtual void HandleTradeWithdrawReq(MemTradeWithdrawMessage* req, std::string* error);
//
//        /**
//            * 处理撤单响应
//            * @params rep: 撤单响应消息
//            */
//        virtual void HandleTradeWithdrawRep(const co::fbs::TradeWithdrawMessage* rep);
//
//        /**
//            * 处理成交回报
//            * @params knock: 成交回报
//            */
//        virtual void OnTradeKnock(const co::fbs::TradeKnock* knock);

    };
}  // namespace co