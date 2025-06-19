#pragma once
#include <string>
#include "x/x.h"
#include "coral/coral.h"
#include "../../mem_broker/mem_struct.h"

std::string FCCreateQueryAsset(const std::string& id, const std::string& fund_id, int64_t timestamp);
std::string FCCreateQueryPosition(const std::string& id, const std::string& fund_id, int64_t timestamp);
std::string FCCreateOrder(const std::string& id, const std::string& fund_id, int64_t timestamp, const std::string& code, int64_t bs_flag, double price, int64_t volume);
std::string FCCreateBatchOrder(int64_t batch_size, const std::string& fund_id, const std::string& id, int64_t timestamp, const std::string& code, int64_t bs_flag, double price, int64_t volume);
std::string FCCreateWithdraw(const std::string& id, const std::string& fund_id, int64_t timestamp, const std::string& order_no);
std::string FCCreateBatchWithdraw(const std::string& id, const std::string& fund_id, int64_t timestamp, const std::string& batch_no);