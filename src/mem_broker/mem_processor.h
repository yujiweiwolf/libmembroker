#pragma once

#include <string>
#include <queue>
#include <vector>
#include <map>
#include <list>
#include <deque>
#include <memory>
#include <tuple>

#include <boost/asio.hpp>
#include "coral/coral.h"
#include "options.h"
#include "mem_struct.h"

namespace co {
class MemBrokerServer;

class MemProcessor {
 public:
    MemProcessor(MemBrokerServer* server);
    ~MemProcessor();

    void Init(const MemBrokerOptions& opt);
    void Run();

 private:
    int cpu_affinity_ = 0;
    MemBrokerServer* server_ = nullptr;
    x::MMapReader consume_reader_;  // 抢占式读网关的报撤单数据
    x::MMapReader common_reader_;  // 查询请求, api回写的交易数据, 有数据就读取
};
typedef std::shared_ptr<MemProcessor> MemProcessorPtr;
}  // namespace co
