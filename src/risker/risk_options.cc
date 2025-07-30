// Copyright 2025 Fancapital Inc.  All rights reserved.
#include <memory>
#include <vector>
#include "x/x.h"
#include "yaml-cpp/yaml.h"
#include "../../src/risker/risk_options.h"

// 自定义RapidJson内置的ASSET，防止因Json错误导致程序崩溃
#ifndef RAPIDJSON_ASSERT
#define RAPIDJSON_ASSERT(x) \
    if (!(x)) {throw std::runtime_error("[FAN-Coral-JsonError]");}
#endif

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace co {

class RiskOptions::RiskOptionsImpl {
 public:
    std::string id_;
    int64_t timestamp_ = 0;
    std::string fund_id_;
    std::string risker_id_;
    bool disabled_ = false;
    std::shared_ptr<co::fbs::TradeAccountT> account_;
    std::string data_;
    rapidjson::Document doc_;
};

RiskOptions::RiskOptions() : m_(new RiskOptionsImpl()) {
}

RiskOptions::~RiskOptions() {
    delete m_;
}

std::vector<std::shared_ptr<RiskOptions>> RiskOptions::Load(const std::string& filename) {
    auto getStr = [&](const YAML::Node& node, const std::string& name) {
        try {
            return node[name] && !node[name].IsNull() ? node[name].as<std::string>() : "";
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
    std::vector<std::shared_ptr<RiskOptions>> risk_opts;
    YAML::Node root = YAML::LoadFile(filename);
    auto risk = root["risk"];
    if (risk["accounts"] && !risk["accounts"].IsNull()) {
        for (auto item : risk["accounts"]) {
            std::shared_ptr<RiskOptions> opt = std::make_shared<RiskOptions>();
            std::string fund_id = getStr(item, "fund_id");
            std::string risker_id = getStr(item, "risker_id");
            std::string name = getStr(item, "name");
            std::string json = getStr(item, "data");

            bool disabled = getBool(item, "disabled");
            bool enable_prevent_self_knock = getBool(item, "enable_prevent_self_knock");
            bool only_etf_anti_self_knock = getBool(item, "only_etf_anti_self_knock");
            opt->set_risker_id(risker_id);
            opt->set_fund_id(fund_id);
            opt->set_disabled(disabled);
            std::string data = "{\"enable_prevent_self_knock\":" + string(enable_prevent_self_knock ? "true" : "false") +
                               "," + "\"only_etf_anti_self_knock\":" + string(only_etf_anti_self_knock ? "true" : "false") +
                               "," + "\"name\":" + "\"" +  name + "\""
                                                                  "," + json + "}";
            opt->set_data(data);
            risk_opts.push_back(opt);
        }
    }
    return risk_opts;
}

std::string RiskOptions::id() const {
    return m_->id_;
}

void RiskOptions::set_id(std::string id) {
    m_->id_ = id;
}

int64_t RiskOptions::timestamp() const {
    return m_->timestamp_;
}

void RiskOptions::set_timestamp(int64_t timestamp) {
    m_->timestamp_ = timestamp;
}

std::string RiskOptions::fund_id() const {
    return m_->fund_id_;
}

void RiskOptions::set_fund_id(std::string fund_id) {
    m_->fund_id_ = fund_id;
}

std::string RiskOptions::risker_id() const {
    return m_->risker_id_;
}

void RiskOptions::set_risker_id(std::string risker_id) {
    m_->risker_id_ = risker_id;
}

bool RiskOptions::disabled() const {
    return m_->disabled_;
}

void RiskOptions::set_disabled(bool disabled) {
    m_->disabled_ = disabled;
}

std::shared_ptr<co::fbs::TradeAccountT> RiskOptions::account() {
    return m_->account_;
}

void RiskOptions::set_account(std::shared_ptr<co::fbs::TradeAccountT> account) {
    m_->account_ = account;
}

std::string RiskOptions::data() const {
    return m_->data_;
}

void RiskOptions::set_data(std::string data) {
    m_->data_ = data;
    if (!data.empty()) {
        m_->doc_.Parse(m_->data_.data(), m_->data_.size());
        if (m_->doc_.HasParseError()) {
            throw std::invalid_argument("parse risk options failed");
        } else if (!m_->doc_.IsObject()) {
            throw std::invalid_argument("parse risk options failed");
        }
    }
}

std::string RiskOptions::GetStr(const std::string& key) const {
    std::string ret;
    auto name = key.c_str();
    if (m_->doc_.HasMember(name)) {
        const rapidjson::Value& value = m_->doc_[name];
        if (value.IsString()) {
            ret = std::string(value.GetString(), value.GetStringLength());
            ret = x::Trim(ret);
        } else if (!value.IsNull()) {
            std::stringstream ss;
            ss << "RiskOptions::GetStr type error: name = " << key;
            throw std::runtime_error(ss.str());
        }
    }
    return ret;
}

int64_t RiskOptions::GetInt64(const std::string& key) const {
    int64_t ret = 0;
    auto name = key.c_str();
    if (m_->doc_.HasMember(name)) {
        const rapidjson::Value& value = m_->doc_[name];
        if (value.IsInt64()) {
            ret = value.GetInt64();
        } else if (!value.IsNull()) {
            std::stringstream ss;
            ss << "RiskOptions::GetInt64 type error: name = " << key;
            throw std::runtime_error(ss.str());
        }
    }
    return ret;
}

double RiskOptions::GetFloat64(const std::string& key) const {
    double ret = 0;
    auto name = key.c_str();
    if (m_->doc_.HasMember(name)) {
        const rapidjson::Value& value = m_->doc_[name];
        if (value.IsDouble()) {
            ret = value.GetDouble();
        } else if (value.IsInt64()) {
            ret = value.GetInt64();
        } else if (!value.IsNull()) {
            std::stringstream ss;
            ss << "RiskOptions::GetFloat64 type error: name = " << key;
            throw std::runtime_error(ss.str());
        }
    }
    return ret;
}

bool RiskOptions::GetBool(const std::string& key) const {
    bool ret = 0;
    auto name = key.c_str();
    if (m_->doc_.HasMember(name)) {
        const rapidjson::Value& value = m_->doc_[name];
        if (value.IsBool()) {
            ret = value.GetBool();
        } else if (!value.IsNull()) {
            std::stringstream ss;
            ss << "RiskOptions::GetBool type error: name = " << key;
            throw std::runtime_error(ss.str());
        }
    }
    return ret;
}
}  // namespace co
