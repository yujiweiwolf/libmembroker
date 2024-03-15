#include "compress_queue.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <boost/lockfree/queue.hpp>
#include <mutex>
#include <condition_variable>

#include "x/x.h"

namespace co {

    class CompressQueueItem {
    public:
        CompressQueueItem(int action, const int64_t& type, const std::string& data):
            action_(action), type_(type), data_(data) {

        }

        inline int action() const {
            return action_;
        }

        inline int64_t type() const {
            return type_;
        }

        inline std::string data() const {
            return data_;
        }

    private:
        int action_ = 0;
        int64_t type_ = 0;
        std::string data_;
    };

    class CompressQueue::CompressQueueImpl {
    public:
        CompressQueueImpl();
        ~CompressQueueImpl();
        void Clear();

        std::atomic_int64_t size_ = 0;
        boost::lockfree::queue<CompressQueueItem*, boost::lockfree::fixed_sized<false>> queue_;

        std::mutex mutex_;
        std::condition_variable cv_;
    };

    CompressQueue::CompressQueueImpl::CompressQueueImpl(): queue_(10000) {

    }

    CompressQueue::CompressQueueImpl::~CompressQueueImpl() {
        Clear();
    }

    CompressQueue::CompressQueue(): m_(new CompressQueueImpl()) {}

    CompressQueue::~CompressQueue() {
        delete m_;
    }

    int64_t CompressQueue::Size() const {
        return m_->size_;
    }

    bool CompressQueue::Empty() const {
        return m_->queue_.empty();
    }

    void CompressQueue::Push(int action, const int64_t& type, const std::string& data) {
        auto item = new CompressQueueItem(action, type, data);
        while (!m_->queue_.push(item)) {
            // pass
        }
        ++m_->size_;
    }

    int64_t CompressQueue::Pop(int* action, std::string* raw, int64_t timeout_ns, int64_t sleep_ns) {
        // timeout_ns = 0 立即返回
        // timeout_ns > 0 超时返回
        // timeout_ns < 0 有数据才返回
        int64_t type = 0;
        int64_t begin_time = 0;
        while (true) {
            CompressQueueItem* item = nullptr;
            if (m_->queue_.pop(item)) {
                --m_->size_;
                (*action) = item->action();
                type = item->type();
                (*raw) = item->data();
                delete item;
                break;
            }
            if (timeout_ns == 0) {
                break;
            } else if (timeout_ns > 0) {
                int64_t now = x::NSTimestamp();
                if (begin_time <= 0) {
                    begin_time = now;
                } else {
                    int64_t ns = now - begin_time;
                    if (ns >= timeout_ns) {
                        break;
                    }
                }
            }
            if (sleep_ns > 0) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_ns));
            }
        }
        return type;
    }

//    void CompressQueue::Wait() {
//        std::unique_lock<std::mutex> lock(m_->mutex_);
//        m_->cv_.wait(lock, [&]{return !m_->queue_.empty();});
//    }

    void CompressQueue::CompressQueueImpl::Clear() {
        while (!queue_.empty()) {
            CompressQueueItem* item = nullptr;
            if (queue_.pop(item)) {
                --size_;
                delete item;
            }
        }
    }

    void CompressQueue::Clear() {
        m_->Clear();
    }

}