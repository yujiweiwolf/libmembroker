#include "upstream.h"

#include <regex>
#include <iomanip>

#include "x/x.h"
#include "coral/coral.h"

#include "mem_server.h"
// #include "utils.h"

//namespace co {
//
//    Upstream::Upstream(int index, BrokerServer* server, boost::asio::io_context* ioc):
//        id_("gateway-" + std::to_string(index)),
//        tag_("<" + id_ +"> "),
//        server_(server),
//        ioc_(ioc),
//        resolver_(*ioc),
//        socket_(*ioc),
//        restart_timer_(*ioc),
//        read_timer_(*ioc),
//        write_timer_(*ioc),
//        await_timer_(*ioc) {
//
//        restart_timer_.expires_at(boost::asio::steady_timer::time_point::max());
//        read_timer_.expires_at(boost::asio::steady_timer::time_point::max());
//        write_timer_.expires_at(boost::asio::steady_timer::time_point::max());
//        await_timer_.expires_at(boost::asio::steady_timer::time_point::max());
//    }
//
//    Upstream::~Upstream() {
//
//    }
//
//    void Upstream::Init(const std::string& address) {
//        x::URL url;
//        x::URL::Parse(&url, address);
//        host_ = url.host();
//        port_ = url.port();
//        cluster_token_ = url.GetStr("cluster_token");
//        compress_algo_ = url.GetBool("compress") ? kCompressAlgoGZip : 0;
//        if (compress_algo_ != 0) {
//            auto self(shared_from_this());
//            compress_thread_ = std::make_shared<std::thread>(&Upstream::RunCompress, self);
//        }
//    }
//
//    void Upstream::SetTradeRegisterMessage(std::shared_ptr<co::fbs::TradeRegisterMessageT> msg) {
//        register_msg_ = msg;
//    }
//
//    void Upstream::Go() {
//        Start();
//    }
//
//    void Upstream::RunCompress() {
//        // 数据压缩线程，保证压缩后的消息顺序与压缩之前是一致的，避免因消息顺序混乱导致业务层受影响，
//        // 比如出现老数据覆盖新数据的情况；
//        // 注意：只有在跨机房时才启用压缩，因为启用压缩后会影响性能；
//        auto self(shared_from_this());
//        int64_t timeout_ns = 1000000;  // 1ms
//        int64_t sleep_ns = 500000; // 500us
//        while (!quit_) {
//            try {
//                std::string raw;
//                int action = 0;
//                int64_t function_id = compress_queue_.Pop(&action, &raw, timeout_ns, sleep_ns);
//                if (function_id != 0) {
//                    if (action == kCompressActionReadPlain) {
//                        ioc_->post([this, self, function_id, raw]{
//                            IOHandleMessage(function_id, raw);
//                        });
//                    } else if (action == kCompressActionReadUncompress) {
//                        raw = GZipDecode(raw);
//                        ioc_->post([this, self, function_id, raw]{
//                            IOHandleMessage(function_id, raw);
//                        });
//                    } else if (action == kCompressActionWriteCompress) {
//                        std::string frame = CreateFrame(function_id, raw, compress_algo_);
//                        ioc_->post([this, self, frame] {
//                            IOSendFrame(frame);
//                        });
//                    } else {
//                        LOG_ERROR << "unknown compress action: " << action;
//                    }
//                }
//            } catch (std::exception& e) {
//                LOG_ERROR << tag_ << "run compress error: " << e.what();
//            }
//        }
//    }
//
//    bool Upstream::IsOpen() {
//        return socket_.is_open();
//    }
//
//    void Upstream::Close() {
//        alive_ = false;
//        boost::system::error_code ec;
//        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
//        socket_.close(ec);
//        restart_timer_.expires_at(boost::asio::steady_timer::time_point::max());
//        read_timer_.expires_at(boost::asio::steady_timer::time_point::max());
//        write_timer_.expires_at(boost::asio::steady_timer::time_point::max());
//        await_timer_.expires_at(boost::asio::steady_timer::time_point::max());
//    }
//
//    void Upstream::Start() {
//        LOG_INFO << tag_ << "connect to gateway ...";
//        Close();
//        restart_timer_.expires_at(boost::asio::steady_timer::time_point::max());
//        read_timer_.expires_after(std::chrono::milliseconds(heartbeat_timeout_ms_));
//        write_timer_.expires_at(boost::asio::steady_timer::time_point::max());
//        await_timer_.expires_at(boost::asio::steady_timer::time_point::max());
//        DoResolve();
//        CheckReadTimer(read_timer_);
//    }
//
//    void Upstream::RestartLater(const std::string& error) {
//        if (quit_) {
//            return;
//        }
//        Close();
//        LOG_ERROR << tag_ << error << ", reconnect in 3s ...";
//        auto self(shared_from_this());
//        restart_timer_.expires_after(std::chrono::milliseconds(3000));
//        restart_timer_.async_wait([this, self](const boost::system::error_code& ec) {
//            if (ec || quit_) {  // Operation canceled
//                return;
//            }
//            restart_timer_.expires_at(boost::asio::steady_timer::time_point::max());
//            Start();
//        });
//    }
//
//    void Upstream::CheckReadTimer(boost::asio::steady_timer& timer) {
//        auto self(shared_from_this());
//        timer.async_wait([this, self, &timer](const boost::system::error_code& /*ec*/) {
//            if (!IsOpen()) {
//                return;
//            }
//            if (timer.expiry() <= boost::asio::steady_timer::clock_type::now()) {
//                std::stringstream ss;
//                ss << "heartbeat timeout for " << heartbeat_timeout_ms_ << "ms";
//                RestartLater(ss.str());
//            } else {
//                CheckReadTimer(timer); // Put the actor back to sleep.
//            }
//        });
//    }
//
//    void Upstream::CheckWriteTimer(boost::asio::steady_timer& timer) {
//        auto self(shared_from_this());
//        timer.async_wait([this, self, &timer](const boost::system::error_code& /*ec*/) {
//            if (!IsOpen()) {
//                return;
//            }
//            if (timer.expiry() <= boost::asio::steady_timer::clock_type::now()) {
//                RestartLater("send timeout for 60s");
//            } else {
//                CheckWriteTimer(timer); // Put the actor back to sleep.
//            }
//        });
//    }
//
//    void Upstream::AwaitOutput() {
//        auto self(shared_from_this());
//        await_timer_.async_wait(
//                [this, self](const boost::system::error_code& /*ec*/) {
//                    if (!IsOpen()) {
//                        return;
//                    }
//                    if (!output_queue_.empty()) {
//                        should_send_heartbeat_ = false;
//                        std::string frame = output_queue_.front();
//                        output_queue_.pop_front();
//                        DoWrite(frame);
//                    } else {
//                        if (should_send_heartbeat_) {
//                            should_send_heartbeat_ = false;
//                            flatbuffers::FlatBufferBuilder fbb;
//                            co::fbs::HeartbeatBuilder builder(fbb);
//                            builder.add_timestamp(x::RawDateTime());
//                            fbb.Finish(builder.Finish());
//                            std::string raw((const char*)fbb.GetBufferPointer(), fbb.GetSize());
//                            std::string frame = CreateFrame(kFuncFBHeartbeat, raw, compress_algo_);
//                            output_queue_.push_back(frame);
//                            await_timer_.expires_at(boost::asio::steady_timer::time_point::min());
//                        } else {
//                            should_send_heartbeat_ = true;
//                            await_timer_.expires_after(std::chrono::milliseconds(heartbeat_interval_ms_));
//                        }
//                        AwaitOutput();
//                    }
//                });
//    }
//
//    void Upstream::DoResolve() {
//        read_timer_.expires_after(std::chrono::milliseconds(10000));
//        boost::asio::ip::tcp::resolver::query query(boost::asio::ip::tcp::v4(), host_, std::to_string(port_));
//        auto self(shared_from_this());
//        resolver_.async_resolve(query,
//                                [this, self](const boost::system::error_code& ec,
//                                        const boost::asio::ip::tcp::resolver::results_type& endpoints) {
//            if (ec) {
//                std::stringstream ss;
//                ss << "resolve error: address = tcp://" << host_ << ":" << port_ << ", error = " << ec.message();
//                std::string error = ss.str();
//                RestartLater(error);
//            } else {
//                DoConnect(endpoints);
//            }
//        });
//    }
//
//    void Upstream::DoConnect(boost::asio::ip::tcp::resolver::results_type endpoints) {
//        auto self(shared_from_this());
//        boost::asio::async_connect(socket_, endpoints,
//                                   [this, self](const boost::system::error_code& ec,
//                                           const boost::asio::ip::tcp::endpoint& endpoint) {
//            if (ec) {
//                std::stringstream ss;
//                ss << "connect error: address = tcp://" << host_ << ":" << port_ << ", error = " << ec.message();
//                std::string error = ss.str();
//                RestartLater(error);
//            } else {
//                DoWriteHandshake();
//            }
//        });
//    }
//
//    void Upstream::DoWriteHandshake() {
//        // 握手消息格式：<handshake_token(32B)><compress_algo(1B)><heartbeat_interval_ms(4B)><heartbeat_timeout_ms(4B)>
//        flatbuffers::FlatBufferBuilder fbb;
//        int32_t heartbeat_interval_ms = x::HostToNet((int32_t)heartbeat_interval_ms_);
//        int32_t heartbeat_timeout_ms = x::HostToNet((int32_t)heartbeat_timeout_ms_);
//        std::stringstream ss;
//        ss << co::kHandshakeToken;
//        ss.put(compress_algo_);
//        ss.write((const char*)&heartbeat_interval_ms, sizeof(int32_t));
//        ss.write((const char*)&heartbeat_timeout_ms, sizeof(int32_t));
//        {  // 向上游交易网关注册，告诉上游当前交易网关支持哪些资金账号的交易；
//            register_msg_->id = x::UUID();
//            register_msg_->timestamp = x::RawDateTime();
//            register_msg_->cluster_token = cluster_token_;
//            fbb.Clear();
//            fbb.Finish(co::fbs::TradeRegisterMessage::Pack(fbb, register_msg_.get()));
//            std::string req_raw((const char*)fbb.GetBufferPointer(), fbb.GetSize());
//            std::string frame = CreateFrame(kFuncFBTradeRegisterReq, req_raw, compress_algo_);
//            ss << frame;
//        }
//        std::string frames = ss.str();
//        wbuf_ = frames;
//        auto self(shared_from_this());
//        boost::asio::async_write(socket_,
//                                 boost::asio::buffer(wbuf_),
//                                 [this, self](const boost::system::error_code& ec, std::size_t /*n*/) {
//            if (!IsOpen()) {
//                return;
//            } else if (ec) {
//                RestartLater("write error: " + ec.message());
//            } else {
//                DoReadHeader();
//            }
//        });
//    }
//
//    void Upstream::DoReadHeader() {
//        // 消息格式：[<body_size><compress_algo><function_id>][<fb>]
//        // [<body_size><compress_algo><function_id>] 为消息头，[<fb>] 为消息体
//        // <body_size>表示后续消息体的字节数
//        // <compress_algo>表示消息体压缩算法
//        // <function_id>表示消息体的功能号
//        // <fb>表示序列化后的数据
//        read_timer_.expires_after(std::chrono::milliseconds(heartbeat_timeout_ms_));
//        size_t read_size = 17; // sizeof(int64_t) + 1 + sizeof(int64_t);
//        rbuf_.resize(read_size);
//        auto self(shared_from_this());
//        boost::asio::async_read(socket_,
//                                boost::asio::buffer(rbuf_),
//                                boost::asio::transfer_exactly(read_size),
//                                [this, self](const boost::system::error_code& ec, std::size_t /*n*/) {
//            if (!IsOpen()) {
//                return;
//            } else if (ec) {
//                RestartLater("read error: " + ec.message());
//                return;
//            }
//            int64_t body_size = 0;
//            memcpy(&body_size, rbuf_.data(), sizeof(int64_t));
//            body_size = x::NetToHost(body_size);
//            if (body_size < (int64_t)sizeof(int64_t) || body_size > kMaxBodyBytes) {
//                std::stringstream ss;
//                ss << "read error: illegal body size: " << body_size << ", limit is " << kMaxBodyBytes;
//                RestartLater(ss.str());
//                return;
//            }
//            char compress_algo = rbuf_.at(sizeof(int64_t));
//            if (compress_algo != 0 && compress_algo != kCompressAlgoGZip) {
//                std::stringstream ss;
//                ss << "read error: unrecognized compress algorighm: " << (int)compress_algo;
//                RestartLater(ss.str());
//                return;
//            }
//            int64_t function_id = 0;
//            memcpy(&function_id, rbuf_.data() + 9, sizeof(int64_t));
//            function_id = x::NetToHost(function_id);
//            DoReadBody(body_size, compress_algo, function_id);
//        });
////        boost::asio::async_read(socket_,
////                                boost::asio::buffer(rbuf_),
////                                boost::asio::transfer_exactly(read_size),
////                                boost::bind(&Upstream::OnReadHeader, self, _1, _2));
//    }
//
//    void Upstream::DoReadBody(int64_t body_size, char compress_algo, int64_t function_id) {
//        rbuf_.resize(body_size);
//        auto self(shared_from_this());
//        boost::asio::async_read(socket_,
//                                boost::asio::buffer(rbuf_),
//                                boost::asio::transfer_exactly(body_size),
//                                [this, self, body_size, compress_algo, function_id](
//                                        const boost::system::error_code& ec, std::size_t /*n*/) {
//            if (!IsOpen()) {
//                return;
//            } else if (ec) {
//                RestartLater(std::string("read error: ") + ec.message());
//                return;
//            }
//            std::string body = rbuf_;
//            if (compress_algo == 0) { // 没有启用压缩
//                if (compress_algo == kCompressAlgoGZip) {
//                    body = GZipDecode(body);
//                }
//                IOHandleMessage(function_id, body);
//            } else {  // 启用了压缩，加入解压缩队列
//                int action = compress_algo == kCompressAlgoGZip ? kCompressActionReadUncompress : kCompressActionReadPlain;
//                compress_queue_.Push(action, function_id, body);
//            }
//            DoReadHeader();
//        });
////        boost::asio::async_read(socket_,
////                                boost::asio::buffer(rbuf_),
////                                boost::asio::transfer_exactly(body_size),
////                                boost::bind(&Upstream::OnReadBody, self, _1, _2));
//    }
//
//
//    void Upstream::DoWrite(const std::string& frame) {
//        auto self(shared_from_this());
//        write_timer_.expires_after(std::chrono::milliseconds(60000)); // 设置Write超时时间
//        wbuf_ = frame;
//        boost::asio::async_write(socket_,
//                                 boost::asio::buffer(wbuf_),
//                                 [this, self](const boost::system::error_code& ec, std::size_t /*n*/) {
//            write_timer_.expires_at(boost::asio::steady_timer::time_point::max());
//            if (!IsOpen()) {
//                return;
//            } else if (ec) {
//                RestartLater("write error: " + ec.message());
//            } else {
//                AwaitOutput();
//            }
//        });
//    }
//
//    void Upstream::OnAlive() {
//        // 删除队列中的上传消息:
//        std::remove_if(output_queue_.begin(), output_queue_.end(), [](std::string& frame) -> bool {
//            int64_t function_id = ParseFunctionIDFromFrame(frame);
//            if (function_id == kFuncFBPushDataReq) {
//                return true;
//            }
//            return false;
//        });
//        // 将已发送但未接收到响应的推送消息重新打散后加入等待队列，
//        // 资金和持仓类消息直接丢掉，因为断线重连后会进行一次全量上传操作；
//        for (auto& itr: pushed_reqs_) {
//            auto raw = itr.second;
//            auto qs = flatbuffers::GetRoot<co::fbs::PushDataMessage>(raw.data());
//            auto items = qs->items();
//            if (items) {
//                for (auto item: *items) {
//                    if (item->type() != kFuncFBTradeAsset &&
//                    item->type() != kFuncFBTradeAssets &&
//                    item->type() != kFuncFBTradePosition &&
//                    item->type() != kFuncFBTradePositions) {
//                        auto arr = std::make_unique<co::fbs::TypedByteArrayT>();
//                        arr->type = item->type();
//                        arr->data.resize(item->data()->size());
//                        memcpy(arr->data.data(), item->data()->data(), item->data()->size());
//                        push_queue_.emplace_front(std::move(arr));
//                    }
//                }
//            }
//        }
//        pushed_reqs_.clear();
//        // 添加资金和持仓全量更新消息到推送队列中
//        server_->SendUploadAllTradeAssetReq(this);
//        server_->SendUploadAllTradePositionReq(this);
//        // 开始Write
//        write_timer_.expires_at(boost::asio::steady_timer::time_point::max());
//        await_timer_.expires_at(boost::asio::steady_timer::time_point::min());
//        AwaitOutput();
//        CheckWriteTimer(write_timer_);
//        alive_ = true;
//        LOG_INFO << tag_ << "connect to gateway successfully: node_id = " << node_id_ << ", node_name = " << node_name_;
//    }
//
//    void Upstream::SendMessage(const int64_t& function_id, const std::string& raw) {
//        if (compress_algo_ == 0) {  // 没有启用压缩
//            std::string frame = CreateFrame(function_id, raw, compress_algo_);
//            auto self(shared_from_this());
//            ioc_->post([this, self, frame]{
//                IOSendFrame(frame);
//            });
//        } else {  // 启用了压缩
//            compress_queue_.Push(kCompressActionWriteCompress, function_id, raw);
//        }
//    }
//
//    void Upstream::IOSendMessage(const int64_t& function_id, const std::string& raw) {
//        if (compress_algo_ == 0) {  // 没有启用压缩
//            std::string frame = CreateFrame(function_id, raw, compress_algo_);
//            IOSendFrame(frame);
//        } else {  // 启用了压缩
//            compress_queue_.Push(kCompressActionWriteCompress, function_id, raw);
//        }
//    }
//
//    void Upstream::IOSendFrame(const std::string& frame) {
//        output_queue_.emplace_back(frame);
//        await_timer_.expires_at(boost::asio::steady_timer::time_point::min());
//    }
//
//    void Upstream::PushData(const int64_t& type, const std::string& raw) {
//        auto self(shared_from_this());
//        ioc_->post([this, self, type, raw]() mutable {
//            IOPushData(type, raw);
//        });
//    }
//
//    void Upstream::IOPushData(const int64_t& type, const std::string& raw) {
//        auto arr = std::make_unique<co::fbs::TypedByteArrayT>();
//        arr->type = type;
//        arr->data.resize(raw.size());
//        memcpy(arr->data.data(), raw.data(), raw.size());
//        push_queue_.emplace_back(std::move(arr));
//        IOTrySendPushData();
//    }
//
//    void Upstream::IOTrySendPushData() {
//        if (!push_queue_.empty() && pushed_reqs_.size() < max_push_pending_size_) {
//            // 从推送队列中取出数据，打包发送
//            co::fbs::PushDataMessageT req;
//            req.id = x::UUID();
//            req.timestamp = x::RawDateTime();
//            int64_t bytes = 0;
//            int64_t limit_bytes_ = 32 << 20; // 32MB
//            while (!push_queue_.empty() && bytes < limit_bytes_) {
//                auto arr = std::move(push_queue_.front());
//                push_queue_.pop_front();
//                bytes += arr->data.size();
//                req.items.emplace_back(std::move(arr));
//            }
//            fbb_.Clear();
//            fbb_.Finish(co::fbs::PushDataMessage::Pack(fbb_, &req));
//            std::string req_raw((const char*)fbb_.GetBufferPointer(), fbb_.GetSize());
//            pushed_reqs_[req.id] = req_raw;
//            IOSendMessage(kFuncFBPushDataReq, req_raw);
//        }
//    }
//
//    void Upstream::IOHandleMessage(const int64_t& function_id, const std::string& raw) {
//        switch (function_id) {
//            case kFuncFBTradeRegisterRep: {
//                auto rep = flatbuffers::GetRoot<co::fbs::TradeRegisterMessage>(raw.data());
//                std::string error = rep->error() ? rep->error()->str() : "";
//                if (!error.empty()) {
//                    RestartLater("register error: " + error);
//                    return;
//                } else {
//                    node_type_ = rep->node_type();
//                    node_id_ = rep->node_id() ? rep->node_id()->str() : "";
//                    node_name_ = rep->node_name() ? rep->node_name()->str() : "";
//                    OnAlive();
//                }
//                break;
//            }
//            case kFuncFBHeartbeat: {
//                auto q = flatbuffers::GetRoot<co::fbs::Heartbeat>(raw.data());
//                heartbeat_delay_ = x::SubRawDateTime(x::RawDateTime(), q->timestamp());
//                break;
//            }
//            case kFuncFBPushDataRep: {
//                auto rep = flatbuffers::GetRoot<co::fbs::PushDataMessage>(raw.data());
//                std::string id = rep->id() ? rep->id()->str() : "";
//                pushed_reqs_.erase(id);
//                IOTrySendPushData();
//                break;
//            }
//            case kFuncFBTradeOrderReq: {
//                server_->SendTradeOrderReq(this, raw);
//                break;
//            }
//            case kFuncFBTradeWithdrawReq: {
//                server_->SendTradeWithdrawReq(this, raw);
//                break;
//            }
//            case kFuncFBGetTradeMarketAssetReq: {
//                server_->SendGetTradeMarketAssetReq(this, raw);
//                break;
//            }
//            case kFuncFBTransferTradeAssetReq: {
//                server_->SendTransferTradeAssetReq(this, raw);
//                break;
//            }
//            default: {
//                LOG_ERROR << tag_ << "unknown function_id: " <<  function_id;
//                break;
//            }
//        }
//    }
//
//    void Upstream::UploadTradeAsset(const std::string& raw) {
//        auto self(shared_from_this());
//        ioc_->post([this, self, raw](){
//            IOPushData(kFuncFBTradeAsset, raw);
//        });
//    }
//
//    void Upstream::UploadTradePositions(const std::string& raw) {
//        auto self(shared_from_this());
//        ioc_->post([this, self, raw](){
//            IOPushData(kFuncFBTradePositions, raw);
//        });
//    }
//
//    void Upstream::UploadTradePolicy(const std::string& raw) {
//        auto self(shared_from_this());
//        ioc_->post([this, self, raw](){
//            IOPushData(kFuncFBTradePolicy, raw);
//        });
//    }
//
//    void Upstream::UploadTradeKnock(const std::string& raw) {
//        auto self(shared_from_this());
//        ioc_->post([this, self, raw](){
//            IOPushData(kFuncFBTradeKnock, raw);
//        });
//    }
//
//    void Upstream::UploadRiskMessage(const std::string& raw) {
//        auto self(shared_from_this());
//        ioc_->post([this, self, raw](){
//            IOPushData(kFuncFBRiskMessage, raw);
//        });
//    }
//
//    void Upstream::SendTradeOrderRep(const std::string& raw) {
//        // 为了确保上游交易网关一定能接收到委托响应，使用可靠推送的方式发送；
//        auto self(shared_from_this());
//        ioc_->post([this, self, raw](){
//            IOPushData(kFuncFBTradeOrderRep, raw);
//        });
//    }
//
//    void Upstream::SendTradeWithdrawRep(const std::string& raw) {
//        auto self(shared_from_this());
//        ioc_->post([this, self, raw](){
//            IOSendMessage(kFuncFBTradeWithdrawRep, raw);
//        });
//    }
//
//    void Upstream::SendGetTradeMarketAssetRep(const std::string& raw) {
//        auto self(shared_from_this());
//        ioc_->post([this, self, raw](){
//            IOSendMessage(kFuncFBGetTradeMarketAssetRep, raw);
//        });
//    }
//
//    void Upstream::SendTransferTradeAssetRep(const std::string& raw) {
//        auto self(shared_from_this());
//        ioc_->post([this, self, raw](){
//            IOSendMessage(kFuncFBTransferTradeAssetRep, raw);
//        });
//    }
//
//    bool Upstream::alive() const {
//        return alive_;
//    }
//
//    std::string Upstream::node_id() const {
//        return node_id_;
//    }
//
//    std::string Upstream::node_name() const {
//        return node_name_;
//    }
//
//}