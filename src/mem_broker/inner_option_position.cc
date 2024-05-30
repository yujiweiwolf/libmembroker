// Copyright 2021 Fancapital Inc.  All rights reserved.
#include "inner_option_position.h"

namespace co {
    InnerOptionPosition::InnerOptionPosition(std::string fund_id, std::string code, int64_t bs_flag): fund_id_(fund_id), code_(code), bs_flag_(bs_flag) {
    }

    std::string InnerOptionPosition::ToString() {
        std::stringstream ss;
        ss << "InnerPosition{";
        ss << "code: " << code_
            << ", bs_flag: " << bs_flag_
            << ", yd_volume: " << yd_volume_
            << ", yd_closing_volume: " << yd_closing_volume_
            << ", yd_close_volume: " << yd_close_volume_
            << ", td_volume: " << td_volume_
            << ", td_closing_volume: " << td_closing_volume_
            << ", td_close_volume: " << td_close_volume_
            << ", td_opening_volume: " << td_opening_volume_
            << ", td_open_volume: " << td_open_volume_
            << "}";
        return ss.str();
    }
}  // namespace co


