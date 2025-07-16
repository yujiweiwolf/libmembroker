#include "utils.h"
#include <regex>
#include <string>
#include <boost/lexical_cast.hpp>

namespace co {

    std::string SafeGBKToUTF8(const std::string& str) {
		// 尝试使用GBK编码进行转换，如果转换失败则判定是否已经是UTF-8编码，如果是就原样放回，否则返回空字符串；
		// 该函数主要用于转换：证券名称 等非关键字段，即使为空也对业务无重大影响。
		// 不能用于转换错误信息，因为如果返回空字符串则可能会导致忽略掉严重错误。
		string ret;
        try {
			ret = x::GBKToUTF8(str);
        } catch (...) {
            ret = str;
        }
		return ret;
    }

    std::string DropCodeSuffix(const std::string& code) {
		string ret = code;
		int pos = (int)code.rfind('.');
        if (pos >= 0) {
			ret = code.substr(0, pos);
        }
        return ret;
    }

    std::string AddCodeSuffix(const std::string& code, const int64_t& market) {
        int pos = (int)code.rfind('.');
        if (pos >= 0) {
            return code;
        }
        auto suffix = MarketToSuffix(market);
        std::stringstream ss;
        ss << code << suffix;
        return ss.str();
    }

    std::string CreateStandardOrderNo(int64_t market, const std::string_view& order_no) {
        // 将市场代码添加到委托合同号中，便于流控模块识别；
        // std_order_no = <market>-<order_no>
        if (market <= 0) {
            throw std::invalid_argument("create standard order_no failed because of illegal market: " +
                                        std::to_string(market));
        }
        std::string std_order_no;
        std_order_no.reserve(order_no.size() + 8);
        std_order_no.append(std::to_string(market));
        std_order_no.append(1, '-');
        std_order_no.append(order_no);
        return std_order_no;
    }

    std::string ParseStandardOrderNo(const std::string_view& order_no, int64_t* market) {
        // std_order_no = <market>-<real_order_no>
        try {
            if (order_no.size() >= 3 && order_no[0] >= '0' && order_no[0] <= '9' && order_no[1] == '-') {
                // 高频出现的分支，特殊处理，提高性能；
                if (market) {
                    (*market) = (int)(order_no[0] - '0');
                }
                return std::string(order_no.substr(2));
            }
            int pos = (int)order_no.find('-');
            if (pos <= 0) {
                throw std::invalid_argument("illegal standard order_no: " + std::string(order_no));
            }
            if (market) {
                (*market) = x::ToInt64(order_no.substr(0, pos));
            }
            std::string real_order_no(order_no.substr(pos + 1));
            return real_order_no;
        } catch (...) {
            throw std::invalid_argument("parse standard order_no failed, order_no: " + std::string(order_no));
        }
    }

    std::string CreateStandardBatchNo(int64_t market, int64_t batch_size, const std::string_view& batch_no) {
        // 将市场代码和批次大小添加到批次号中，便于流控模块识别
        // std_batch_no = <market>-<batch_size>-<real_batch_no>
        if (market <= 0) {
            throw std::invalid_argument("create standard batch_no failed because of illegal market: " +
                                        std::to_string(market));
        }
        if (batch_size <= 0) {
            throw std::invalid_argument("create standard batch_no failed because of illegal batch_size: " +
                                        std::to_string(batch_size));
        }
        std::string std_batch_no;
        std_batch_no.reserve(batch_no.size() + 10);
        std_batch_no.append(std::to_string(market));
        std_batch_no.append(1, '-');
        std_batch_no.append(std::to_string(batch_size));
        std_batch_no.append(1, '-');
        std_batch_no.append(batch_no);
        return std_batch_no;
    }

    std::string ParseStandardBatchNo(const std::string_view& batch_no, int64_t* market, int64_t* batch_size) {
        // std_batch_no = <market>-<batch_size>-<real_batch_no>
        try {
            int pos1 = (int)batch_no.find('-');
            if (pos1 <= 0) {
                throw std::invalid_argument("");
            }
            int pos2 = (int)batch_no.find('-', pos1 + 1);
            if (pos2 <= 0) {
                throw std::invalid_argument("");
            }
            if (market) {
                (*market) = x::ToInt64(batch_no.substr(0, pos1));
            }
            if (batch_size) {
                (*batch_size) = x::ToInt64(batch_no.substr(pos1 + 1, pos2 - pos1 - 1));
            }
            std::string real_batch_no(batch_no.substr(pos2 + 1));
            return real_batch_no;
        } catch (std::exception& e) {
            throw std::invalid_argument("parse standard batch_no failed, batch_no: " + std::string(batch_no));
        }
    }

    std::string CreateInnerOrderNo(const co::fbs::TradeOrderT& order) {
        return order.order_no;
    }

    std::string CreateInnerMatchNo(const co::fbs::TradeKnockT& knock) {
        // 最早期的股票内部成交合同号生成规则：<order_no>_<code>_<match_no>_<match_time>
        // ETF申赎操作，一个委托合同号对应多条不同代码的成交回报数据
        // 注意：自成交match_no是一样的，系统根据match_no是否重复来检查是否有自成交。
        std::string inner_match_no;
        switch (knock.trade_type) {
            case kTradeTypeSpot: {  //
                std::stringstream ss;
                ss << knock.order_no << "_" << knock.match_no << "_" << knock.code;
                inner_match_no = ss.str();
                break;
            }
            case kTradeTypeFuture:  //
            case kTradeTypeOption:  //
            default: {
                std::stringstream ss;
                if (!knock.match_no.empty()) {
                    ss << knock.bs_flag << "_" << knock.match_no;
                }
                inner_match_no = ss.str();
                break;
            }
        }
       return inner_match_no;
    }

    std::string CheckTradeOrderMessage(MemTradeOrderMessage *req, int sh_th_tps_limit, int sz_th_tps_limit) {
        if (req->items_size <= 0) {
            return "[FAN-BROKER-ERROR] items_size is zero";
        }
        auto first = req->items;
        int64_t first_market = 0;
        for (int i = 0; i < req->items_size; ++i) {
            auto order = first + i;
            if (strlen(order->code) == 0) {
                return "[FAN-BROKER-ERROR] code is required";
            }
            int64_t market = order->market;
            if (market <= 0) {
                market = co::CodeToMarket(order->code);
                order->market = market;
            }
            if (market <= 0) {
                std::stringstream ss;
                return ("[FAN-BROKER-ERROR] unknown market suffix in code: " + string(order->code));
            }
            int max_batch_size = 0;
            if (market == co::kMarketSH) {
                max_batch_size = sh_th_tps_limit;
            } else if (market == co::kMarketSZ) {
                max_batch_size = sz_th_tps_limit;
            }
            if (max_batch_size > 0 && req->items_size > max_batch_size) {
                return ("[FAN-BROKER-ERROR] too many orders in request: " + std::to_string(req->items_size) + ", up limit is: " + std::to_string(max_batch_size));
            }

            if (i == 0) {
                first_market = market;
            }
            if (i > 0 && market != first_market) {
                std::stringstream ss;
                ss << "[FAN-BROKER-ERROR] all orders must have the same market: order[0].code="
                   << first->code << ", order[" << i << "].code=" << order->code;
                return ss.str();
            }
            // --------------------------------------------------------------
            //只允许放一天的逆回购
            if (req->bs_flag == co::kBsFlagSell) {
                if (market == kMarketSH && strncmp(order->code, "204", 3) == 0) {
                    if (strcmp(order->code, "204001.SH") != 0) {
                        return ("[FAN-Broker-RepoRiskError] only 1-Day repo code is allowed: " + string(order->code));
                    }
                } else if (market == kMarketSZ && strncmp(order->code, "1318", 4) == 0) {
                    if (strcmp(order->code, "131810.SZ") != 0) {
                        return ("[FAN-Broker-RepoRiskError] only 1-Day repo code is allowed: " + string(order->code));
                    }
                }
            }
        }
        return "";
    }

    std::string CheckTradeWithdrawMessage(MemTradeWithdrawMessage *req, int64_t trade_type) {
        if (strlen(req->order_no) == 0 && strlen(req->batch_no) == 0) {
            return ("[FAN-BROKER-ERROR] order_no and batch_no both empty");
        }
        if (trade_type == kTradeTypeSpot) {
            if (strlen(req->order_no)) {
                if (req->market == 0) {
                    if (req->order_no[0] == '1') {
                        req->market = co::kMarketSH;
                    } else if (req->order_no[0] == '2') {
                        req->market = co::kMarketSZ;
                    }
                }
//                string order_no = req->order_no;
//                std::smatch result;
//                bool flag = regex_match(order_no, result, std::regex("^(1|2|3|9)-(.*)"));
//                if (!flag) {
//                    return ("[FAN-BROKER-ERROR] not valid order_no: " + order_no);
//                }
                if ((req->order_no[0] < '1' || req->order_no[0] > '9') || req->order_no[1] != '-') {
                    return ("[FAN-BROKER-ERROR] not valid order_no: " + string(req->order_no));
                }
                return "";
            } else if (strlen(req->batch_no)) {
                if (req->market == 0) {
                    if (req->batch_no[0] == '1') {
                        req->market = co::kMarketSH;
                    } else if (req->batch_no[0] == '2') {
                        req->market = co::kMarketSZ;
                    }
                }
                string batch_no = req->batch_no;
                std::smatch result;
                bool flag = regex_match(batch_no, result, std::regex("^(1|2|3|9)-([0-9]{1,3})-(.*)"));
                if (!flag) {
                    return ("[FAN-BROKER-ERROR] not valid batch_no: " + batch_no);
                }
            }
        }
        return "";
    }

    std::string GenerateRandomString(size_t length) {
        const std::string charset ="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::random_device rd;
        std::mt19937 generator(rd());
        std::uniform_int_distribution<> distribution(0, charset.size() - 1);
        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result += charset[distribution(generator)];
        }
        return result;
    }
}