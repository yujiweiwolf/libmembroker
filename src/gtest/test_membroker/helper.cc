#include "helper.h"
//using namespace co;

std::string FCCreateQueryAsset(const std::string& id, const std::string& fund_id, int64_t timestamp) {
    char buffer[sizeof(co::MemGetTradeAssetMessage)] = "";
    co::MemGetTradeAssetMessage* req = (co::MemGetTradeAssetMessage*)buffer;
    req->timestamp = timestamp;
    strncpy(req->id, id.c_str(), id.length());
    strncpy(req->fund_id, fund_id.c_str(), fund_id.length());
    std::string raw = string(static_cast<const char*>(buffer), sizeof(co::MemTradeWithdrawMessage));
    return raw;
}

std::string FCCreateQueryPosition(const std::string& id, const std::string& fund_id, int64_t timestamp) {
    char buffer[sizeof(co::MemGetTradePositionMessage)] = "";
    co::MemGetTradePositionMessage* req = (co::MemGetTradePositionMessage*)buffer;
    req->timestamp = timestamp;
    strncpy(req->id, id.c_str(), id.length());
    strncpy(req->fund_id, fund_id.c_str(), fund_id.length());
    std::string raw = string(static_cast<const char*>(buffer), sizeof(co::MemGetTradePositionMessage));
    return raw;
}

std::string FCCreateOrder(const std::string& id, const std::string& fund_id, int64_t timestamp, const std::string& code, int64_t bs_flag, double price, int64_t volume) {
    int length = sizeof(co::MemTradeOrderMessage) + sizeof(co::MemTradeOrder);
    char buffer[length] = "";
    co::MemTradeOrderMessage* req = (co::MemTradeOrderMessage*)buffer;
    req->timestamp = timestamp;
    strncpy(req->id, id.c_str(), id.length());
    strncpy(req->fund_id, fund_id.c_str(), fund_id.length());
    req->bs_flag = bs_flag;
    req->items_size = 1;
    co::MemTradeOrder* items = req->items;
    co::MemTradeOrder* order = items + 0;
    strcpy(order->code, code.c_str());
    order->market = co::CodeToMarket(code);
    order->price = price;
    order->volume = volume;
    std::string raw = string(static_cast<const char*>(buffer), length);
    return raw;
}

std::string FCCreateBatchOrder(int64_t batch_size, const std::string& id, const std::string& fund_id, int64_t timestamp, const std::string& code, int64_t bs_flag, double price, int64_t volume) {
    int length = sizeof(co::MemTradeOrderMessage) + sizeof(co::MemTradeOrder) * batch_size;
    char buffer[length] = "";
    co::MemTradeOrderMessage* req = (co::MemTradeOrderMessage*)buffer;
    req->timestamp = timestamp;
    strncpy(req->id, id.c_str(), id.length());
    strncpy(req->fund_id, fund_id.c_str(), fund_id.length());
    req->bs_flag = bs_flag;
    req->items_size = batch_size;
    co::MemTradeOrder* items = req->items;
    for (int64_t i = 0; i < batch_size; i++) {
        co::MemTradeOrder* order = items + i;
        strcpy(order->code, code.c_str());
        order->market = co::CodeToMarket(code);
        order->price = price;
        order->volume = volume;
    }
    std::string raw = string(static_cast<const char*>(buffer), length);
    return raw;
}

std::string FCCreateWithdraw(const std::string& id, const std::string& fund_id, int64_t timestamp, const std::string& order_no) {
    co::MemTradeWithdrawMessage req = {};
    req.timestamp = timestamp;
    strncpy(req.id, id.c_str(), id.length());
    strncpy(req.fund_id, fund_id.c_str(), fund_id.length());
    strncpy(req.order_no, order_no.c_str(), order_no.length());
    std::string raw = string(reinterpret_cast<char*>(&req), sizeof(req));
    return raw;
}

std::string FCCreateBatchWithdraw(const std::string& id, const std::string& fund_id, int64_t timestamp, const std::string& batch_no) {
    co::MemTradeWithdrawMessage req = {};
    req.timestamp = timestamp;
    strncpy(req.id, id.c_str(), id.length());
    strncpy(req.fund_id, fund_id.c_str(), fund_id.length());
    strncpy(req.batch_no, batch_no.c_str(), batch_no.length());
    std::string raw = string(reinterpret_cast<char*>(&req), sizeof(req));
    return raw;
}