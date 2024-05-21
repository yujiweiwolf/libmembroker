#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <x/x.h>

namespace co {
class InnerStockOrder {
 public:
    InnerStockOrder() = default;
    ~InnerStockOrder() = default;

    inline int64_t oc_flag() {
        return oc_flag_;
    }
    inline void set_oc_flag(int64_t v) {
        oc_flag_ = v;
    }
    inline int64_t bs_flag() {
        return bs_flag_;
    }
    inline void set_bs_flag(int64_t v) {
        bs_flag_ = v;
    }
    inline int64_t order_volume() {
        return order_volume_;
    }
    inline void set_order_volume(int64_t v) {
        order_volume_ = v;
    }
    inline int64_t match_volume() {
        return match_volume_;
    }
    inline void set_match_volume(int64_t v) {
        match_volume_ = v;
    }
    inline int64_t withdraw_volume() {
        return withdraw_volume_;
    }
    inline void set_withdraw_volume(int64_t v) {
        withdraw_volume_ = v;
    }

 private:
    // 委托类型：oc=0,bs=1-正常买入；oc=0,bs=2-正常卖出；oc=1,bs=2-融券卖出；oc=2,bs=1-买券还券
    int64_t oc_flag_ = 0;
    int64_t bs_flag_ = 0;
    int64_t order_volume_ = 0;        // 委托数量
    int64_t match_volume_ = 0;        // 成交数量
    int64_t withdraw_volume_ = 0;    // 撤单数量（废单情况下，认为是全部撤单）
};
    typedef std::shared_ptr<InnerStockOrder> InnerStockOrderPtr;
}  // namespace co
