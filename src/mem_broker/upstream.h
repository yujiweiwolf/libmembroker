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
#include "compress_queue.h"

namespace co {

    class MemBrokerServer;

    /**
     * 上游交易网关通信
     * 1.向上游上传每个资金账号的交易数据：资金、持仓、委托、成交；
     * 2.接收上游的交易指令并返回响应；
     * 3.服务器启动并加装完所有数据后，上传全量的资金、持仓、委托和成交数据；
     * 4.断线重连后，上传全量的资金和持仓数据；
     *
     */
    class Upstream : public std::enable_shared_from_this<Upstream> {
    public:
        Upstream(int index, MemBrokerServer* server, boost::asio::io_context* ioc);
        ~Upstream();

        void Init(const std::string& address);
        void SetTradeRegisterMessage(std::shared_ptr<co::fbs::TradeRegisterMessageT> msg);

        void Go();
        void UploadTradeAsset(const std::string& raw);
        void UploadTradePositions(const std::string& raw);
        void UploadTradePolicy(const std::string& raw);
        void UploadTradeKnock(const std::string& raw);
        void UploadRiskMessage(const std::string& raw);

        void SendTradeOrderRep(const std::string& raw);
        void SendTradeWithdrawRep(const std::string& raw);
        void SendGetTradeMarketAssetRep(const std::string& raw);
        void SendTransferTradeAssetRep(const std::string& raw);

        bool alive() const;
        std::string node_id() const;
        std::string node_name() const;

    protected:
        bool IsOpen();
        void Close();
        void Start();
        void RestartLater(const std::string& error);

        void RunCompress();

        void OnAlive();
        void PushData(const int64_t& type, const std::string& raw);
        void IOPushData(const int64_t& type, const std::string& raw);
        void IOTrySendPushData();
        void SendMessage(const int64_t& function_id, const std::string& raw);
        void IOSendMessage(const int64_t& function_id, const std::string& raw);
        void IOSendFrame(const std::string& raw);

        void CheckReadTimer(boost::asio::steady_timer& timer);
        void CheckWriteTimer(boost::asio::steady_timer& timer);
        void AwaitOutput();
        void DoWrite(const std::string& frame);

        void DoResolve();
        void DoConnect(boost::asio::ip::tcp::resolver::results_type endpoints);
        void DoWriteHandshake();
        void DoReadHeader();
        //void OnReadHeader(const boost::system::error_code& ec, std::size_t n);
        void DoReadBody(int64_t body_size, char compress_algo, int64_t function_id);
        //void OnReadBody(const boost::system::error_code& ec, std::size_t n);
        void IOHandleMessage(const int64_t& function_id, const std::string& raw);

    private:
        std::string id_;
        std::string tag_;

        int64_t node_type_ = 0;
        std::string node_id_; // 节点编号
        std::string node_name_; // 节点名称

        bool quit_ = false;
        bool alive_ = false;

        flatbuffers::FlatBufferBuilder fbb_;

        std::shared_ptr<co::fbs::TradeRegisterMessageT> register_msg_ = nullptr;

        std::string host_;
        int port_ = 0;
        std::string cluster_token_;
        int64_t heartbeat_interval_ms_ = 10000; // 心跳间隔时间
        int64_t heartbeat_timeout_ms_ = 60000; // 心跳超时时间
        int64_t heartbeat_delay_ = 0;
        char compress_algo_ = 0;

        MemBrokerServer* server_ = nullptr;
        boost::asio::io_context* ioc_ = nullptr;
        boost::asio::ip::tcp::resolver::results_type endpoints_;
        boost::asio::ip::tcp::resolver resolver_;
        boost::asio::ip::tcp::socket socket_;
        boost::asio::steady_timer restart_timer_;
        boost::asio::steady_timer read_timer_;
        boost::asio::steady_timer write_timer_;
        boost::asio::steady_timer await_timer_;
        bool should_send_heartbeat_ = false;

        std::string rbuf_;
        std::string wbuf_;
        std::list<std::string> output_queue_;

        size_t max_push_pending_size_ = 1; // 最大推送等待响应的消息个数
        std::list<std::unique_ptr<co::fbs::TypedByteArrayT>> push_queue_;
        std::map<std::string, std::string> pushed_reqs_; // 已发送等待响应的DataPushMessage，msg_id -> msg
        std::shared_ptr<std::thread> push_thread_ = nullptr;

        CompressQueue compress_queue_;
        std::shared_ptr<std::thread> compress_thread_ = nullptr;
    };

    typedef std::shared_ptr<Upstream> UpstreamPtr;

}