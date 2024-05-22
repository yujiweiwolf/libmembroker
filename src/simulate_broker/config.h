#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <x/x.h>
#include "../mem_broker//options.h"
#include "../mem_broker/mem_struct.h"

using namespace std;

namespace co {

    class Config {
    public:
        static Config* Instance();

        inline MemBrokerOptionsPtr options() {
            return options_;
        }

        inline const std::map<std::string, std::shared_ptr<MemTradeAccount>>& accounts() const {
            return accounts_;
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
    };
}