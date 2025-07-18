// Copyright 2021 Fancapital Inc.  All rights reserved.
#pragma once
#include <map>
#include <memory>
#include <x/x.h>
#include "../mem_broker/options.h"
#include "../mem_broker/mem_struct.h"
#include "../risker/risk_options.h"

using namespace std;

namespace co {
class Config {
 public:
    static Config* Instance();

    MemBrokerOptionsPtr options() {
        return options_;
    }

    const std::map<std::string, std::shared_ptr<MemTradeAccount>>& accounts() const {
        return accounts_;
    }

    const std::vector<std::shared_ptr<RiskOptions>>& risk_opt() const {
        return risk_opts_;
    }

 protected:
    Config() = default;
    ~Config() = default;
    Config(const Config&) = delete;
    const Config& operator=(const Config&) = delete;

    void Init();

 private:
    static Config* instance_;
    MemBrokerOptionsPtr options_;
    std::map<std::string, std::shared_ptr<MemTradeAccount>> accounts_;
    std::vector<std::shared_ptr<RiskOptions>> risk_opts_;
};
}  // namespace co
