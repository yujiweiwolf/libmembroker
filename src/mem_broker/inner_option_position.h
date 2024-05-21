#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include "coral/coral.h"

namespace co {
class InnerOptionPosition {
 public:
    InnerOptionPosition(std::string fund_id, std::string code, int64_t bs_flag);

    std::string ToString();

    inline std::string fund_id() {
        return fund_id_;
    }
    inline std::string code() {
        return code_;
    }
    inline int64_t bs_flag() {
        return bs_flag_;
    }
    inline int64_t yd_volume() {
        return yd_volume_;
    }
    inline void set_yd_volume(int64_t v) {
        yd_volume_ = v;
    }
    inline int64_t yd_closing_volume() {
        return yd_closing_volume_;
    }
    inline void set_yd_closing_volume(int64_t v) {
        yd_closing_volume_ = v;
    }
    inline int64_t yd_close_volume() {
        return yd_close_volume_;
    }
    inline void set_yd_close_volume(int64_t v) {
        yd_close_volume_ = v;
    }
    inline int64_t td_volume() {
        return td_volume_;
    }
    inline void set_td_volume(int64_t v) {
        td_volume_ = v;
    }
    inline int64_t td_closing_volume() {
        return td_closing_volume_;
    }
    inline void set_td_closing_volume(int64_t v) {
        td_closing_volume_ = v;
    }
    inline int64_t td_close_volume() {
        return td_close_volume_;
    }
    inline void set_td_close_volume(int64_t v) {
        td_close_volume_ = v;
    }
    inline int64_t td_opening_volume() {
        return td_opening_volume_;
    }
    inline void set_td_opening_volume(int64_t v) {
        td_opening_volume_ = v;
    }
    inline int64_t td_open_volume() {
        return td_open_volume_;
    }
    inline void set_td_open_volume(int64_t v) {
        td_open_volume_ = v;
    }

 private:
    std::string fund_id_;
    std::string code_;
    int64_t bs_flag_ = 0; // 买卖标记：1-买入，2-卖出
    int64_t yd_volume_ = 0; // 昨日持仓
    int64_t yd_closing_volume_ = 0; // 昨日持仓平仓冻结数
    int64_t yd_close_volume_ = 0; // 昨日持仓已平仓数
    int64_t td_volume_ = 0; // 今日持仓
    int64_t td_closing_volume_ = 0; // 今日持仓平仓冻结数
    int64_t td_close_volume_ = 0; // 今日持仓已平仓数
    int64_t td_opening_volume_ = 0; // 今日持仓开仓冻结数
    int64_t td_open_volume_ = 0; // 今日持仓已开仓数
};
    typedef std::shared_ptr<InnerOptionPosition> InnerOptionPositionPtr;
}  // namespace co
