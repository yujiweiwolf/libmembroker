// Copyright 2021 Fancapital Inc.  All rights reserved.
#include <unordered_map>
#include "yaml-cpp/yaml.h"
#include "coral/coral.h"
#include "../../src/risker/risk_master.h"
#include "common/anti_self_knock_risker.h"
#include "fancapital/fancapital_risker.h"

const char kRiskerFancapital[] = "fancapital";
const char kVersion[] = "v2.0.1";

namespace co {
constexpr int kAsyncStateIdle = 0;
constexpr int kAsyncStateRunning = 1;
constexpr int kAsyncStateDone = 2;

class RiskMaster::RiskMasterImpl {
 public:
    RiskMasterImpl();
    ~RiskMasterImpl();
    void Init(const std::vector<std::shared_ptr<RiskOptions>>& opts);
    void Start();
    void Run();

    void HandleTradeOrderReq(MemTradeOrderMessage* req);

    void HandleTradeOrderRep(MemTradeOrderMessage* rep);

    void HandleTradeWithdrawReq(MemTradeWithdrawMessage* req);

    void HandleTradeWithdrawRep(MemTradeWithdrawMessage* rep);

    void OnTradeKnock(MemTradeKnock* knock);

    void OnTick(MemQTickBody* tick);

    std::vector<Risker*>* GetRiskers(const std::string& fund_id);

    // fund_id -> [账户风控，公共风控1，公共风控2, ...]
    std::unordered_map<std::string, std::vector<Risker*>*> routes_;
    AntiSelfKnockRisker anti_risker_;

    StringQueue trade_queue_;
    StringQueue feed_queue_;

    std::atomic_int8_t async_state_ = 0;  // 0-空转，1-运行中，2-已结束
    std::string async_result_;

    std::shared_ptr<std::thread> thread_;
    bool debug_ = false;
};

    RiskMaster::RiskMasterImpl::RiskMasterImpl() {
    }

    RiskMaster::RiskMasterImpl::~RiskMasterImpl() {
        for (auto& itr : routes_) {
            auto list = itr.second;
            delete list;
        }
    }

    RiskMaster::RiskMaster() : m_(new RiskMasterImpl()) {
    }

    RiskMaster::~RiskMaster() {
        delete m_;
    }

    void RiskMaster::RiskMasterImpl::Init(const std::vector<std::shared_ptr<RiskOptions>>& opts) {
        LOG_INFO << "librisk kVersion: " << kVersion;
        bool need_feeder = false;
        for (auto& opt : opts) {
            if (opt->disabled()) {
                continue;
            }
            std:string fund_id = opt->fund_id();
            std::string risker_id = opt->risker_id();

            // ---------------------------------------------------
            // 创建账户风控
            // ---------------------------------------------------
            Risker* account_risker = nullptr;
            if (risker_id == kRiskerFancapital) {
                account_risker = new FancapitalRisker();
            } else {
                LOG_ERROR << "[risk] unknown risker_id: " << risker_id << ", fund_id: " << fund_id;
                continue;
            }

            auto riskers = new std::vector<Risker*>();
            if (account_risker) {
                account_risker->Init(opt);
                riskers->push_back(account_risker);
            }

            // ---------------------------------------------------
            // 创建公共风控
            // ---------------------------------------------------
            // 防对敲
            bool enable_prevent_self_knock = opt->GetBool("enable_prevent_self_knock");
            if (enable_prevent_self_knock) {
                need_feeder = true;
                anti_risker_.AddOption(opt);
                riskers->push_back(&anti_risker_);
            }
            routes_[fund_id] = riskers;
        }
        try {
            std::string filename = x::FindFile("risker.yaml");
            YAML::Node root = YAML::LoadFile(filename);
            LOG_INFO << "[risk][master] load configuration ok";
        } catch (std::exception& e) {
            LOG_ERROR << "[risk][master] load risker configuration failed: " << e.what();
        }
    }

    void RiskMaster::RiskMasterImpl::Start() {
        thread_ = std::make_shared<std::thread>(std::bind(&RiskMaster::RiskMasterImpl::Run, this));
    }

    void RiskMaster::RiskMasterImpl::Run() {
        try {
            std::string raw;
            int64_t type = 0;
            while (true) {
                while (!trade_queue_.Empty()) {
                    type = trade_queue_.Pop(&raw);
                    if (type == 0) {
                        break;
                    }
                    switch (type) {
                        case kMemTypeTradeOrderReq: {
                            MemTradeOrderMessage *req = reinterpret_cast<MemTradeOrderMessage*>(raw.data());
                            std::string fund_id = req->fund_id;
                            std::string error;
                            auto riskers = GetRiskers(fund_id);
                            if (riskers) {
                                for (auto& risker : *riskers) {
                                    error = std::move(risker->HandleTradeOrderReq(req));
                                    if (!error.empty()) {
                                        break;
                                    }
                                }
                                if (error.empty()) {
                                    for (auto& risker : *riskers) {
                                        risker->OnTradeOrderReqPass(req);
                                    }
                                }
                            }
                            if (!error.empty()) {
                                if (error[0] != '[') {
                                    error = "[FAN-RISK-Error] " + error;
                                }
                            }
                            if (async_state_.load() != kAsyncStateRunning) {
                                throw std::runtime_error("unexpected async state");
                            }
                            async_result_ = std::move(error);
                            async_state_.store(kAsyncStateDone);
                            break;
                        }
                        case kFuncFBTradeWithdrawReq: {
                            MemTradeWithdrawMessage *req = reinterpret_cast<MemTradeWithdrawMessage*>(raw.data());
                            std::string fund_id = req->fund_id;
                            std::string error;
                            auto riskers = GetRiskers(fund_id);
                            if (riskers) {
                                for (auto& risker : *riskers) {
                                    error = risker->HandleTradeWithdrawReq(req);
                                    if (!error.empty()) {
                                        break;
                                    }
                                }
                                if (error.empty()) {
                                    for (auto& risker : *riskers) {
                                        risker->OnTradeWithdrawReqPass(req);
                                    }
                                }
                            }
                            if (!error.empty()) {
                                if (error[0] != '[') {
                                    error = "[FAN-RISK-Error] " + error;
                                }
                            }
                            if (async_state_.load() != kAsyncStateRunning) {
                                throw std::runtime_error("unexpected async state");
                            }
                            async_result_ = std::move(error);
                            async_state_.store(kAsyncStateDone);
                            break;
                        }
                        default: {
                            break;
                        }
                    }
                }
            }
        } catch (std::exception& e) {
            LOG_ERROR << "[risk][master] risk master is crashed: " << e.what();
        }
    }

    std::vector<Risker*>* RiskMaster::RiskMasterImpl::GetRiskers(const std::string& fund_id) {
        std::vector<Risker*>* riskers = nullptr;
        auto itr = routes_.find(fund_id);
        if (itr != routes_.end()) {
            riskers = itr->second;
        }
        return riskers;
    }

    void RiskMaster::RiskMasterImpl::HandleTradeOrderRep(MemTradeOrderMessage* rep) {
        auto riskers = GetRiskers(rep->fund_id);
        if (riskers) {
            for (auto& risker : *riskers) {
                risker->HandleTradeOrderRep(rep);
            }
        }
    }

    void RiskMaster::RiskMasterImpl::HandleTradeWithdrawRep(MemTradeWithdrawMessage* rep) {
        std::string fund_id = rep->fund_id;
        auto riskers = GetRiskers(fund_id);
        if (riskers) {
            for (auto& risker : *riskers) {
                risker->HandleTradeWithdrawRep(rep);
            }
        }
    }

    void RiskMaster::RiskMasterImpl::OnTradeKnock(MemTradeKnock* knock) {
        std::string fund_id = knock->fund_id;
        auto riskers = GetRiskers(fund_id);
        if (riskers) {
            for (auto& risker : *riskers) {
                risker->OnTradeKnock(knock);
            }
        }
    }

    void RiskMaster::RiskMasterImpl::OnTick(MemQTickBody* tick) {
        anti_risker_.OnTick(tick);
    }

    void RiskMaster::Init(const std::vector<std::shared_ptr<RiskOptions>>& opts) {
        m_->Init(opts);
    }

    void RiskMaster::HandleTradeOrderReq(MemTradeOrderMessage* req, std::string* error) {
        if (m_->async_state_.load() != kAsyncStateIdle) {
            throw std::runtime_error("unexpected async state");
        }
        m_->async_state_.store(kAsyncStateRunning);
        m_->trade_queue_.Push(kMemTypeTradeOrderReq, string(reinterpret_cast<const char *>(req), sizeof(MemTradeOrderMessage)));
        while (m_->async_state_.load() != kAsyncStateDone) {}
        if (!m_->async_result_.empty()) {
            (*error) = m_->async_result_;
        }
        m_->async_state_.store(kAsyncStateIdle);
    }

    void RiskMaster::HandleTradeWithdrawReq(MemTradeWithdrawMessage* req, std::string* error) {
        if (m_->async_state_.load() != kAsyncStateIdle) {
            throw std::runtime_error("unexpected async state");
        }
        m_->async_state_.store(kAsyncStateRunning);
        m_->trade_queue_.Push(kMemTypeTradeWithdrawReq, string(reinterpret_cast<const char *>(req), sizeof(MemTradeWithdrawMessage)));
        while (m_->async_state_.load() != kAsyncStateDone) {}
        if (!m_->async_result_.empty()) {
            (*error) = m_->async_result_;
        }
        m_->async_state_.store(kAsyncStateIdle);
    }
}  // namespace co
