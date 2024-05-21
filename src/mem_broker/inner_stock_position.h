#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include "inner_stock_order.h"

namespace co {
    class InnerStockPosition {
    public:
        InnerStockPosition(std::string fund_id, std::string code);

        string ToString();

        inline std::string code() {
            return code_;
        }
        inline void set_code(const std::string &code) {
            code_ = code;
        }
        inline int64_t borrowed_volume() {
            return borrowed_volume_;
        }
        inline void set_borrowed_volume(int64_t v) {
            borrowed_volume_ = v;
        }
        inline int64_t borrowing_volume() {
            return borrowing_volume_;
        }
        inline void set_borrowing_volume(int64_t v) {
            borrowing_volume_ = v;
        }
        inline int64_t returned_volume() {
            return returned_volume_;
        }
        inline void set_returned_volume(int64_t v) {
            returned_volume_ = v;
        }
        inline int64_t returning_volume() {
            return returning_volume_;
        }
        inline void set_returning_volume(int64_t v) {
            returning_volume_ = v;
        }
        inline int64_t bought_volume() {
            return bought_volume_;
        }
        inline void set_bought_volume(int64_t v) {
            bought_volume_ = v;
        }
        inline int64_t buying_volume() {
            return buying_volume_;
        }
        inline void set_buying_volume(int64_t v) {
            buying_volume_ = v;
        }
        inline int64_t sold_volume() {
            return sold_volume_;
        }
        inline void set_sold_volume(int64_t v) {
            sold_volume_ = v;
        }
        inline int64_t selling_volume() {
            return selling_volume_;
        }
        inline void set_selling_volume(int64_t v) {
            selling_volume_ = v;
        }
        inline int64_t total_borrowed_volume() {
            return total_borrowed_volume_;
        }
        inline void set_total_borrowed_volume(int64_t v) {
            total_borrowed_volume_ = v;
        }
        inline int64_t total_sell_volume() {
            return total_sell_volume_;
        }
        inline void set_total_sell_volume(int64_t v) {
            total_sell_volume_ = v;
        }

    private:
        std::string fund_id_;
        std::string code_;

        int64_t total_borrowed_volume_ = 0;  // 今日融券卖出的总额度
        int64_t borrowed_volume_ = 0;        // 已融券卖出数量
        int64_t borrowing_volume_ = 0;        // 融券卖出冻结数量
        int64_t returned_volume_ = 0;        // 已买券还券数量
        int64_t returning_volume_ = 0;        // 买券还券冻结数量

        int64_t total_sell_volume_ = 0;      // 今日普通卖出的总额度
        int64_t bought_volume_ = 0;            // 普通已买入数量
        int64_t buying_volume_ = 0;            // 普通买入冻结数量
        int64_t sold_volume_ = 0;            // 普通已卖出数量
        int64_t selling_volume_ = 0;        // 普通卖出冻结数量
    };
    typedef std::shared_ptr<InnerStockPosition> InnerStockPositionPtr;
}  // namespace co
