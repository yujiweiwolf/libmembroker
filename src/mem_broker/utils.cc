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
//        if (market <= 0) {
//            throw std::invalid_argument("create standard order_no failed because of illegal market: " +
//                std::to_string(market));
//        }
//        std::string std_order_no;
//        switch (market) {
//            case co::kMarketSH:
//                if (order_no.size() >= 3 && order_no[0] == 'S' && order_no[1] == 'H' && order_no[2] == '-') {
//                    std_order_no = order_no;
//                } else {
//                    std_order_no.reserve(order_no.size() + 4);
//                    std_order_no.append("SH-");
//                    std_order_no.append(order_no);
//                }
//                break;
//            case co::kMarketSZ:
//                if (order_no.size() >= 3 && order_no[0] == 'S' && order_no[1] == 'Z' && order_no[2] == '-') {
//                    std_order_no = order_no;
//                } else {
//                    std_order_no.reserve(order_no.size() + 4);
//                    std_order_no.append("SZ-");
//                    std_order_no.append(order_no);
//                }
//                break;
//            case co::kMarketBJ:
//                if (order_no.size() >= 3 && order_no[0] == 'B' && order_no[1] == 'J' && order_no[2] == '-') {
//                    std_order_no = order_no;
//                } else {
//                    std_order_no.reserve(order_no.size() + 4);
//                    std_order_no.append("BJ-");
//                    std_order_no.append(order_no);
//                }
//                break;
//            default:
//                std_order_no = order_no;
//                break;
//        }
//        return std_order_no;
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
            return "no orders in request";
        }
        auto first = req->items;
        int64_t first_market = 0;
        for (int i = 0; i < req->items_size; ++i) {
            auto order = first + i;
            if (strlen(order->code) == 0) {
                return "code is required";
            }
            int64_t market = co::CodeToMarket(order->code);
            if (market <= 0) {
                std::stringstream ss;
                return ("unknown market suffix in code: " + string(order->code));
            }
            int max_batch_size = 0;
            if (market == co::kMarketSH) {
                max_batch_size = sh_th_tps_limit;
            } else if (market == co::kMarketSH) {
                max_batch_size = sz_th_tps_limit;
            }
            if (max_batch_size > 0 && req->items_size > max_batch_size) {
                return ("too many orders in request: " + std::to_string(req->items_size) + ", up limit is: " + std::to_string(max_batch_size));
            }

            if (i == 0) {
                first_market = market;
            }
            if (i > 0 && market != first_market) {
                std::stringstream ss;
                ss << "all orders must have the same market: order[0].code="
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
            return ("[FAN-BROKER-ERROR] order_no or batch_no is required");
        }
        if (trade_type == kTradeTypeSpot) {
            if (strlen(req->order_no)) {
                string order_no = req->order_no;
                std::smatch result;
                bool flag = regex_match(order_no, result, std::regex("^(1|2|3|9)-(.*)"));
                if (!flag) {
                    return ("not valid order_no: " + order_no);
                }
                return "";
            }
            if (strlen(req->batch_no)) {
                string batch_no = req->batch_no;
                std::smatch result;
                bool flag = regex_match(batch_no, result, std::regex("^(1|2|3|9)-([0-9]{1,3})-(.*)"));
                if (!flag) {
                    return ("not valid order_no: " + batch_no);
                }
            }
        }
        return "";
    }

    int64_t CreateOrderStatus(const co::fbs::TradeOrderT& order) {
        int64_t status = 0;
        if (!order.error.empty()) {
            status = kOrderFailed;
        } else if (order.withdraw_volume > 0) {
            if (order.match_volume + order.withdraw_volume >= order.volume) { // 已撤
                status = kOrderFullyCanceled;
            } else { // 部撤
                status = kOrderPartlyCanceled;
            }
        } else if (order.match_volume > 0) {
            if (order.match_volume + order.withdraw_volume >= order.volume) { // 已成
                status = kOrderFullyKnocked;
            } else { // 部成
                status = kOrderPartlyKnocked;
            }
        } else { // 已报
            status = kOrderCreated;
        }
        return status;
    }

    void Fix(co::fbs::TradeAssetT* asset) {
        // 唯一标示：<timestamp>#<fund_id>#<username>，timestamp只精确到日期
        //std::stringstream ss;
        //ss << (asset->timestamp / 1000000000 * 1000000000)
        //    << "#" << asset->fund_id << "#";
        //asset->id = ss.str();
    }

    void Fix(co::fbs::TradePositionT* position) {
        position->market = co::CodeToMarket(position->code);
    }

    void Fix(co::fbs::TradeKnockT* knock) {
        if (knock->match_type == kMatchTypeWithdrawOK || knock->match_type == kMatchTypeFailed) {
            // 撤单成交和废单成交，将成交合同号统一设置为：_<order_no>
            knock->match_no = "_" + knock->order_no;
        }
        knock->inner_match_no = CreateInnerMatchNo(*knock);
        int64_t ts = FixTimestamp(knock->timestamp);
        if (ts > 0) {
            LOG_WARN << "fix timestamp: " << knock->timestamp << " -> " << ts << ", knock = " << ToString(*knock);
            knock->timestamp = ts;
        }
        knock->market = co::CodeToMarket(knock->code);
    }

    void Validate(const co::fbs::TradeAssetT& asset) {
        if (!x::IsValidRawDateTime(asset.timestamp)) {
            xthrow() << "[FAN-Broker-AssetValidateError] illegal timestamp, asset: " << ToString(asset);
        }
        if (asset.fund_id.empty()) {
            xthrow() << "[FAN-Broker-AssetValidateError] illegal fund_id, asset: " << ToString(asset);
        }
        if (asset.balance < 0) {
            xthrow() << "[FAN-Broker-AssetValidateError] illegal balance, asset: " << ToString(asset);
        }
        if (asset.usable < 0) {
            xthrow() << "[FAN-Broker-AssetValidateError] illegal usable, asset: " << ToString(asset);
        }
        if (asset.usable > asset.balance) {
            xthrow() << "[FAN-Broker-PositionValidateError] logic(usable<=balance), asset: " << ToString(asset);
        }
    }

    void Validate(const co::fbs::TradePositionT& position) {
        if (!x::IsValidRawDateTime(position.timestamp)) {
            xthrow() << "[FAN-Broker-PositionValidateError] illegal timestamp, position: " << ToString(position);
        }
        if (position.fund_id.empty()) {
            xthrow() << "[FAN-Broker-PositionValidateError] illegal fund_id, position: " << ToString(position);
        }
        if (position.code.empty()) {
            xthrow() << "[FAN-Broker-PositionValidateError] illegal code, position: " << ToString(position);
        }
        int64_t market = co::CodeToMarket(position.code);
        if (market <= 0) {
            xthrow() << "[FAN-Broker-PositionValidateError] unknown market in code, position: " << ToString(position);
        }
        if (position.long_volume < 0) {
            xthrow() << "[FAN-Broker-PositionValidateError] illegal long_volume, position: " << ToString(position);
        }
        if (position.long_can_close < 0) {
            xthrow() << "[FAN-Broker-PositionValidateError] illegal long_can_close, position: " << ToString(position);
        }
        if (position.long_volume <= 0 && position.long_market_value != 0) {
            xthrow() << "[FAN-Broker-PositionValidateError] logic(long_market_value<=long_volume*price*multiple), position: " << ToString(position);
        }
        if (position.long_can_close > position.long_volume) {
            xthrow() << "[FAN-Broker-PositionValidateError] logic(long_can_close<=long_volume), position: " << ToString(position);
        }
        if (position.short_volume < 0) {
            xthrow() << "[FAN-Broker-PositionValidateError] illegal short_volume, position: " << ToString(position);
        }
        if (position.short_can_close < 0) {
            xthrow() << "[FAN-Broker-PositionValidateError] illegal short_can_close, position: " << ToString(position);
        }
        if (position.short_volume <= 0 && position.short_market_value != 0) {
            xthrow() << "[FAN-Broker-PositionValidateError] logic(short_market_value<=short_volume*price*multiple), position: " << ToString(position);
        }
        if (position.short_can_close > position.short_volume) {
            xthrow() << "[FAN-Broker-PositionValidateError] logic(short_can_close<=short_volume), position: " << ToString(position);
        }
    }

    void Validate(const co::fbs::TradeKnockT& knock) {
        if (!x::IsValidRawDateTime(knock.timestamp)) {
            xthrow() << "[FAN-Broker-KnockValidateError] illegal timestamp, knock: " << ToString(knock);
        }
        if (knock.fund_id.empty()) {
            xthrow() << "[FAN-Broker-KnockValidateError] illegal fund_id, knock: " << ToString(knock);
        }
        if (knock.trade_type < kTradeTypeSpot || knock.trade_type > kTradeTypeOption) {
            xthrow() << "[FAN-Broker-KnockValidateError] illegal trade_type, knock: " << ToString(knock);
        }
        if (knock.inner_match_no.empty()) {
            xthrow() << "[FAN-Broker-KnockValidateError] illegal inner_match_no, knock: " << ToString(knock);
        }
        if (knock.order_no.empty()) {
            xthrow() << "[FAN-Broker-KnockValidateError] illegal order_no, knock: " << ToString(knock);
        }
        if (knock.code.empty()) {
            xthrow() << "[FAN-Broker-KnockValidateError] illegal code, knock: " << ToString(knock);
        }
        int64_t market = co::CodeToMarket(knock.code);
        if (market <= 0) {
            xthrow() << "[FAN-Broker-KnockValidateError] unknown market in code, knock: " << ToString(knock);
        }
        if (knock.bs_flag < kBsFlagBuy || knock.bs_flag > kBsFlagRedeem) {
            xthrow() << "[FAN-Broker-KnockValidateError] illegal bs_flag, knock: " << ToString(knock);
        }
        if (knock.oc_flag < kOcFlagAuto || knock.oc_flag > kOcFlagLocalForceClose) {
            xthrow() << "[FAN-Broker-KnockValidateError] illegal oc_flag, knock: " << ToString(knock);
        }
        switch (knock.match_type) {
        case kMatchTypeOK:
            if (knock.match_no.empty()) {
                xthrow() << "[FAN-Broker-KnockValidateError] illegal match_no, knock: " << ToString(knock);
            }
            if (knock.match_volume < 0) { // ETF申赎交保证金的情况下，match_volume可能会等于0；
                xthrow() << "[FAN-Broker-KnockValidateError] illegal match_volume, knock: " << ToString(knock);
            }
            break;
        case kMatchTypeWithdrawOK:
            if (knock.match_volume <= 0) {
                xthrow() << "[FAN-Broker-KnockValidateError] illegal match_volume, knock: " << ToString(knock);
            }
            break;
        case kMatchTypeFailed:
            break;
        default:
            xthrow() << "[FAN-Broker-KnockValidateError] illegal match_type, knock: " << ToString(knock);
        }
    }

	int64_t FixTimestamp(const int64_t& timestamp) {
		int64_t date = timestamp / 1000000000LL;
		if (date < 19700101 || date > 30000101) {
			date = x::RawDate();
			int64_t ts = date * 1000000000LL; // 如果时间戳不正确，则返回当前自然日零点的时间戳
			return ts;
		}
		return 0;
	}

    std::string CreateFrame(const int64_t& function_id, const std::string& raw, char compress_algo) {
        // 消息格式：[<body_size><compress_algo><function_id>][<fb>]
        int64_t _function_id = x::HostToNet(function_id);
        string body = raw;
        char _compress_algo = 0x00;
        if (compress_algo == kCompressAlgoGZip && raw.size() > 128) {
            body = x::GZipEncode(raw);
            _compress_algo = kCompressAlgoGZip;
        }
        if ((int64_t)body.size() > kMaxBodyBytes) {
            stringstream ss;
            ss << "create message error: body size is too large: " << body.size() << ", limit is " << kMaxBodyBytes;
            throw runtime_error(ss.str());
        }
        int64_t _body_size = x::HostToNet((int64_t)body.size());
        string frame(17 + body.size(), '\0');
        char* p = (char*)frame.data();
        memcpy(p, (const char*)&_body_size, sizeof(int64_t));
        p[sizeof(int64_t)] = _compress_algo;
        memcpy(p + 9, (const char*)&_function_id, sizeof(int64_t));
        memcpy(p + 17, body.data(), body.size());
        return frame;
    }

    int64_t ParseFunctionIDFromFrame(const string& frame) {
        int64_t function_id = 0;
        if (frame.size() >= 17) {
            memcpy((void*)&function_id, (const void*)(frame.data() + 9), sizeof(int64_t));
            function_id = x::NetToHost(function_id);
        }
        return function_id;
    }

    std::string CreateHeartbeatMessage() {
        flatbuffers::FlatBufferBuilder fbb;
        co::fbs::HeartbeatBuilder builder(fbb);
        builder.add_timestamp(x::RawDateTime());
        auto fbHeartbeat = builder.Finish();
        fbb.Finish(fbHeartbeat);
        string raw((const char*)fbb.GetBufferPointer(), fbb.GetSize());
        return raw;
    }

    void SetTraceReqBegin(flatbuffers::Vector<int64_t>* traces, int64_t ns) {
        // (node_type, req_begin, req_end, rep_begin, rep_end)
        if (traces) {
            for (int i = 0; i + 5 <= (int)traces->size(); i += 5) {
                int64_t node_type = traces->Get(i);
                if (node_type <= 0 || node_type == kTradeNodeTypeBroker) {
                    traces->Mutate(i, kTradeNodeTypeBroker); // node_type
                    traces->Mutate(i + 1, ns); // req_begin
                    break;
                }
            }
        }
    }

    void SetTraceReqEnd(flatbuffers::Vector<int64_t>* traces, int64_t ns) {
        if (traces) {
            for (int i = 0; i + 5 <= (int)traces->size(); i += 5) {
                int64_t node_type = traces->Get(i);
                if (node_type == kTradeNodeTypeBroker) {
                    traces->Mutate(i + 2, ns);
                    break;
                }
            }
        }
    }

    void SetTraceRepBegin(flatbuffers::Vector<int64_t>* traces, int64_t ns) {
        if (traces) {
            for (int i = 0; i + 5 <= (int)traces->size(); i += 5) {
                int64_t node_type = traces->Get(i);
                if (node_type == kTradeNodeTypeBroker) {
                    traces->Mutate(i + 3, ns);
                    break;
                }
            }
        }
    }

    void SetTraceRepEnd(flatbuffers::Vector<int64_t>* traces, int64_t ns, int64_t* req, int64_t* rep, int64_t* out) {
        *req = -1;
        *rep = -1;
        *out = -1;
        if (traces) {
            for (int i = 0; i + 5 <= (int)traces->size(); i += 5) {
                int64_t node_type = traces->Get(i);
                if (node_type == kTradeNodeTypeBroker) {
                    traces->Mutate(i + 4, ns);
                    *req = traces->Get(i + 2) - traces->Get(i + 1); // req_end - req_begin
                    *rep = traces->Get(i + 4) - traces->Get(i + 3); // rep_end - rep_begin
                    *out = traces->Get(i + 3) - traces->Get(i + 2); // rep_begin - req_end
                    break;
                }
            }
        }
    }

}