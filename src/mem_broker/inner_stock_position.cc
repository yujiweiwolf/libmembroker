#include "inner_stock_position.h"

namespace co {

	InnerStockPosition::InnerStockPosition(std::string fund_id, std::string code):
        fund_id_(fund_id),
        code_(code) {

    }

    std::string InnerStockPosition::ToString() {
        stringstream ss;
        ss << "InnerPosition{";
        ss << "code: " << code_
            << ", total_borrowed_volume: " << total_borrowed_volume_
            << ", borrowed_volume: " << borrowed_volume_
            << ", borrowing_volume: " << borrowing_volume_
			<< ", returned_volume: " << returned_volume_
			<< ", returning_volume: " << returning_volume_
            << ", total_sell_volume_: " << total_sell_volume_
            << ", bought_volume: " << bought_volume_
            << ", buying_volume: " << buying_volume_
			<< ", sold_volume: " << sold_volume_
			<< ", selling_volume: " << selling_volume_
            << "}";
        return ss.str();
    }
    
}

