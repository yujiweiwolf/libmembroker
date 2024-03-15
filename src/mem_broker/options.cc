#include <iostream>
#include <sstream>
#include <string>
#include <regex>
#include <filesystem>

#include "yaml-cpp/yaml.h"

#include "options.h"

namespace co {

    namespace fs = std::filesystem;


    std::shared_ptr<MemBrokerOptions> MemBrokerOptions::Load(const std::string& filename) {
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
        auto getInt = [&](const YAML::Node& node, const std::string& name, const int64_t& default_value = 0) {
            try {
                return node[name] && !node[name].IsNull() ? node[name].as<int64_t>() : default_value;
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
        auto opt = std::make_shared<MemBrokerOptions>();
        std::string _filename = filename.empty() ? "broker.yaml" : filename;
        auto file = x::FindFile(_filename);
        opt->log_opt_ = x::LoggingOptions::Load(file);
        x::InitializeLogging(*opt->log_opt_);
        YAML::Node root = YAML::LoadFile(file);
        auto broker = root["broker"];
        opt->wal_ = getStr(broker, "wal");
        opt->trade_gateway_ = getStr(broker, "trade_gateway");
        opt->enable_upload_ = getBool(broker, "enable_upload");
        opt->request_timeout_ms_ = getInt(broker, "request_timeout_ms");
        if (opt->request_timeout_ms_ <= 0) {
            opt->request_timeout_ms_ = 10000;
        }
        opt->enable_stock_short_selling_ = getBool(broker, "enable_stock_short_selling");
        opt->enable_query_only_ = getBool(broker, "enable_query_only");
        opt->query_asset_interval_ms_ = getInt(broker, "query_asset_interval_ms");
        opt->query_position_interval_ms_ = getInt(broker, "query_position_interval_ms");
        opt->query_knock_interval_ms_ = getInt(broker, "query_knock_interval_ms");
        opt->idle_sleep_ns_ = getInt(broker, "idle_sleep_ns");
        opt->cpu_affinity_ = getInt(broker, "cpu_affinity", -1);
        opt->mem_dir_ = getStr(broker, "mem_dir");
        opt->mem_req_file_ = getStr(broker, "mem_req_file");
        opt->mem_rep_file_ = getStr(broker, "mem_rep_file");
        return opt;
    }

    string MemBrokerOptions::ToString() {
        std::regex re("cluster_token=[^&a-zA-Z0-9]*");
        string gw = trade_gateway_;
        std::regex_replace(gw, re, "*");
        stringstream ss;
        ss << "broker:" << std::endl
            << "  trade_gateway: " << gw << std::endl
            << "  wal: " << wal_ << std::endl
            << "  enable_upload: " << (enable_upload_ ? "true" : "false") << std::endl
            << "  query_asset_interval_ms: " << query_asset_interval_ms_ << "ms" << std::endl
            << "  query_position_interval_ms: " << query_position_interval_ms_ << "ms" << std::endl
            << "  query_knock_interval_ms: " << query_knock_interval_ms_ << "ms" << std::endl
            << "  request_timeout_ms: " << request_timeout_ms_ << "ms" << std::endl
            << "  enable_query_only: " << (enable_query_only_ ? "true" : "false") << std::endl
            << "  enable_stock_short_selling: " << (enable_stock_short_selling_ ? "true" : "false") << std::endl
            << "  idle_sleep_ns: " << idle_sleep_ns_ << "ns" << std::endl
            << "  cpu_affinity: " << cpu_affinity_ << std::endl
            << "  mem_dir: " << mem_dir_ << std::endl
            << "  mem_req_file: " << mem_req_file_ << std::endl
            << "  mem_rep_file: " << mem_rep_file_ << std::endl
            << log_opt_->ToString();
        return ss.str();
    }

}
