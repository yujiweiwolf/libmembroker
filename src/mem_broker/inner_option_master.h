#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>

#include "x/x.h"
#include "coral/coral.h"
#include "mem_struct.h"
#include "inner_option_position.h"

namespace co {

    /**
     * 期权内部持仓管理，用于自动开平仓
     * 期权内部持仓的管理和更新全部由框架完成，无需底层参与，底层只需要告知框架是否启用期权自动开平仓功能即可。
     *
     * 工作流程：
     * 0.系统启动前，需要人工确认：将对应账户下所有挂单全部撤单，并在系统初始化完成之前不允许再报单（包括第三方客户端上也不允许报单）；
     * 1.系统启动时，查询对应账号的当前持仓作为初始持仓，完成初始化；（该步骤由Broker框架完成）
     * 2.接收到成交回报，更新内部持仓；（该步骤由Broker框架完成）
     * 3.报单请求时，调用接口获取自动开平仓方向；（该步骤由Broker框架完成）
     * 4.报单成功时，更新内部持仓，冻结数量；（该步骤由Broker框架完成）
     * 5.报单失败时，更新内部持仓，解冻数量；（该步骤由Broker框架完成）
     */
    class InnerOptionMaster {
    public:
        InnerOptionMaster() = default;

        // 清空持仓，用于断线重连后重新初始化
        void Clear();
        // 更新初始持仓
        void SetInitPositions(MemGetTradePositionMessage* rep);
        // 更新委托和成交
        void HandleOrderReq(const std::string& fund_id, int64_t bs_flag, const MemTradeOrder& order);
        void HandleOrderRep(const std::string& fund_id, int64_t bs_flag, const MemTradeOrder& order);
        void HandleKnock(const MemTradeKnock* knock);

        /**
         * 计算自动开平仓方向
         * @param order: 委托
         * @return: 处理后的开平仓标记
         */
        int64_t GetAutoOcFlag(const std::string& fund_id, int64_t bs_flag, const MemTradeOrder& order);

    protected:
        bool IsAccountInitialized(std::string fund_id);
        void Update(InnerOptionPositionPtr pos, int64_t oc_flag, int64_t order_volume, int64_t match_volume, int64_t withdraw_volume);
        std::string GetKey(std::string code, int64_t bs_flag);
        InnerOptionPositionPtr GetPosition(std::string fund_id, std::string code, int64_t bs_flag);
        InnerOptionPositionPtr GetOrCreatePosition(std::string fund_id, std::string code, int64_t bs_flag);

    private:
        std::map<std::string, std::shared_ptr<std::map<std::string, InnerOptionPositionPtr>>> positions_; // <fund_id> -> <code>_<bs_flag> -> obj
        std::unordered_map<std::string, bool> knocks_; // <fund_id>_<inner_match_no> -> true
    };

    typedef std::shared_ptr<InnerOptionMaster> InnerOptionMasterPtr;
}  // namespace co
