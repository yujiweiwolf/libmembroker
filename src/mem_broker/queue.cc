#include "queue.h"

#include <chrono>
#include <thread>

#include <boost/lockfree/queue.hpp>

namespace co {

    BrokerMsg* BrokerMsg::Create(void* worker, const int64_t& function_id, const std::string& data) {
        BrokerMsg* ret = new BrokerMsg();
        ret->set_worker(worker);
        ret->set_function_id(function_id);
        ret->set_data(data);
        return ret;
    }

    void BrokerMsg::Destory(BrokerMsg* data) {
        if (data) {
            delete data;
        }
    }

    class BrokerQueue::BrokerQueueImpl {
    public:
        BrokerQueueImpl();

        std::atomic_int64_t size_ = 0;
        int64_t idle_sleep_ns_ = 0; // 空转时休眠的时间（单位：纳秒）
        boost::lockfree::queue<BrokerMsg*, boost::lockfree::fixed_sized<false>> queue_;
    };

    BrokerQueue::BrokerQueueImpl::BrokerQueueImpl(): queue_(10000) {

    }

    BrokerQueue::BrokerQueue(): m_(new BrokerQueueImpl()) {
    
    }

    BrokerQueue::~BrokerQueue() {
        delete m_;
    }

    void BrokerQueue::SetIdleSleepNS(int64_t us) {
        m_->idle_sleep_ns_ = us;
    }

    int64_t BrokerQueue::Size() const {
        return m_->size_;
    }

    bool BrokerQueue::Empty() const {
        return m_->queue_.empty();
    }

    void BrokerQueue::Push(void* worker, const int64_t& function_id, const std::string& data) {
        BrokerMsg* msg = BrokerMsg::Create(worker, function_id, data);
        while (!m_->queue_.push(msg)) {
            // pass
        }
        ++m_->size_;
    }

    BrokerMsg* BrokerQueue::Pop() {
        BrokerMsg* msg = nullptr;
        while (!m_->queue_.pop(msg)) {
            if (m_->idle_sleep_ns_ > 0) { // 防止CPU过载，进行休眠；
               std::this_thread::sleep_for(std::chrono::nanoseconds(m_->idle_sleep_ns_));
            }
        }
        --m_->size_;
        return msg;
    }

    BrokerMsg* BrokerQueue::TryPop() {
        BrokerMsg* msg = nullptr;
        if (m_->queue_.pop(msg)) {
            --m_->size_;
        }
        return msg;
    }

}