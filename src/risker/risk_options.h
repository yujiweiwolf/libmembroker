// Copyright 2021 Fancapital Inc.  All rights reserved.

#pragma once
#include <string>
#include <memory>

#include "coral/coral.h"

namespace co {

class RiskOptions {
 public:
    RiskOptions();
    ~RiskOptions();
    RiskOptions(const RiskOptions&) = delete;
    RiskOptions& operator=(const RiskOptions&) = delete;
    static std::vector<std::shared_ptr<RiskOptions>> Load(const std::string& filename = "");

    std::string id() const;
    void set_id(std::string id);
    int64_t timestamp() const;
    void set_timestamp(int64_t timestamp);
    std::string fund_id() const;
    void set_fund_id(std::string fund_id);
    std::string risker_id() const;
    void set_risker_id(std::string risker_id);
    bool disabled() const;
    void set_disabled(bool disabled);
    std::shared_ptr<co::fbs::TradeAccountT> account();
    void set_account(std::shared_ptr<co::fbs::TradeAccountT> account);
    std::string data() const;
    void set_data(std::string data);

    std::string GetStr(const std::string& key) const;
    int64_t GetInt64(const std::string& key) const;
    double GetFloat64(const std::string& key) const;
    bool GetBool(const std::string& key) const;

 private:
    class RiskOptionsImpl;
    RiskOptionsImpl* m_ = nullptr;
};

}  // namespace co
