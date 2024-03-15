#include "config.h"
#include "yaml-cpp/yaml.h"
namespace co {

    Config* Config::instance_ = 0;

    Config* Config::Instance() {
        if (instance_ == 0) {
            instance_ = new Config();
            instance_->Init();
        }
        return instance_;
    }

    void Config::Init() {
        auto getStr = [&](const YAML::Node& node, const std::string& name) {
            try {
                return node[name] && !node[name].IsNull() ? node[name].as<std::string>() : "";
            } catch (std::exception& e) {
                LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
                throw std::runtime_error(e.what());
            }
        };
        auto getStrings = [&](std::vector<std::string>* ret, const YAML::Node& node, const std::string& name, bool drop_empty = false) {
            try {
                if (node[name] && !node[name].IsNull()) {
                    for (auto item : node[name]) {
                        std::string s = x::Trim(item.as<std::string>());
                        if (!drop_empty || !s.empty()) {
                            ret->emplace_back(s);
                        }
                    }
                }
            } catch (std::exception& e) {
                LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
                throw std::runtime_error(e.what());
            }
        };
        auto filename = x::FindFile("broker.yaml");
        YAML::Node root = YAML::LoadFile(filename);
        options_ = MemBrokerOptions::Load(filename);
        auto fake = root["fake"];
        if (fake["accounts"] && !fake["accounts"].IsNull()) {
            for (auto item : fake["accounts"]) {
                auto account = std::make_shared<MemTradeAccount>();
                auto route = std::make_unique<co::fbs::TradeRouteT>();
                std::string fund_id = getStr(item, "fund_id");
                std::string s_trade_type = getStr(item, "trade_type");
                int64_t trade_type = 0;
                if (s_trade_type == "spot") {
                    trade_type = kTradeTypeSpot;
                } else if (s_trade_type == "future") {
                    trade_type = kTradeTypeFuture;
                } else if (s_trade_type == "option") {
                    trade_type = kTradeTypeOption;
                } else {
                    throw std::invalid_argument("illegal trade_type: " +s_trade_type + ", e.g. spot/future/option");
                }
                std::vector<std::string> suffixes;
                getStrings(&suffixes, item, "markets");
                std::vector<int64_t> markets;
                for (auto& suffix: suffixes) {
                    try {
                        int64_t market = 1;
                        markets.emplace_back(market);
                    } catch (std::exception& e) {
                        throw std::invalid_argument("unrecognized market suffix: " + suffix);
                    }
                }
                strncpy(account->fund_id, fund_id.c_str(), fund_id.length());
                account->type = trade_type;
//                route->fund_id = fund_id;
//                route->markets.insert(route->markets.begin(), markets.begin(), markets.end());
                accounts_[fund_id] = std::move(account);
            }
        }
        stringstream ss;
        ss << "+-------------------- configuration begin --------------------+" << endl;
        ss << options_->ToString() << endl;
        ss << endl;
        ss << "fake:" << std::endl
           << "  accounts:" << std::endl;
        for (auto& itr: accounts_) {
            auto& acc = itr.second;
            ss << "    - {fund_id: \"" << acc->fund_id << "\", trade_type: \"";
            if (acc->type == kTradeTypeSpot) {
                ss << "spot";
            } else if (acc->type == kTradeTypeFuture) {
                ss << "future";
            } else if (acc->type == kTradeTypeOption) {
                ss << "option";
            } else {
                ss << acc->type << "-unknown";
            }
            ss << std::endl;
        }
        ss << "+-------------------- configuration end   --------------------+";
        LOG_INFO << endl << ss.str();
    }
}