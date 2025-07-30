// Copyright 2025 Fancapital Inc.  All rights reserved.
#include <string>
#include <vector>
#include <utility>
#include "config.h"
#include "yaml-cpp/yaml.h"
namespace co {

Config* Config::instance_ = nullptr;

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
    auto getBool = [&](const YAML::Node& node, const std::string& name) {
        try {
            return node[name] && !node[name].IsNull() ? node[name].as<bool>() : false;
        } catch (std::exception& e) {
            LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
            throw std::runtime_error(e.what());
        }
    };
    auto filename = x::FindFile("broker.yaml");
    YAML::Node root = YAML::LoadFile(filename);
    options_ = MemBrokerOptions::Load(filename);
    auto fake = root["fake"];
    std::string fund_id = getStr(fake, "fund_id");
    std::string s_trade_type = getStr(fake, "trade_type");
    std::string name = getStr(fake, "name");
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
    memset(&account_, 0, sizeof(account_));
    strcpy(account_.fund_id, fund_id.c_str());
    strcpy(account_.name, name.c_str());
    account_.type = trade_type;

    risk_opts_ = RiskOptions::Load(filename);
    stringstream ss;
    ss << "+-------------------- configuration begin --------------------+" << endl;
    ss << options_->ToString() << endl;
    ss << endl;
    ss << "fake: " << std::endl
       << "fund_id: " << account_.fund_id
       << "name: " << account_.name
       << "trade_type: " << s_trade_type;

    ss << "risk:" << std::endl
       << "  accounts:" << std::endl;
    for (auto& risk : risk_opts_) {
        ss << "    - {fund_id: \"" << risk->fund_id() << "\", risker_id: \"" << risk->risker_id();
        ss << "\", disabled: \"" << std::boolalpha << risk->disabled();
        ss << "\", enable_prevent_self_knock: \"" << std::boolalpha << risk->GetBool("enable_prevent_self_knock");
        ss << "\", only_etf_anti_self_knock: \"" << std::boolalpha << risk->GetBool("only_etf_anti_self_knock");
        ss << "\", withdraw_ratio: \""  << risk->GetFloat64("withdraw_ratio");
        ss << "\", knock_ratio: \""  << risk->GetFloat64("knock_ratio");
        ss << "\", failure_ratio: \""  << risk->GetFloat64("failure_ratio");
        ss << "\", max_order_volume: \""  << risk->GetInt64("max_order_volume");
        ss << "\", max_order_amount: \""  << risk->GetFloat64("max_order_amount");
        ss << std::endl;
    }
    ss << "+-------------------- configuration end   --------------------+";
    LOG_INFO << endl << ss.str();
}
}  // namespace co
