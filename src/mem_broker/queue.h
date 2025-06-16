#pragma once

#include <string>
#include <memory>

namespace co {

    class BrokerMsg {
    public:
        static BrokerMsg* Create(void* worker, const int64_t& function_id, const std::string& data);
        static void Destory(BrokerMsg* data);

        inline void* worker() {
            return worker_;
        }

        inline void set_worker(void* worker) {
            worker_ = worker;
        }

        inline int64_t function_id() const {
            return function_id_;
        }

        inline void set_function_id(int64_t function_id) {
            function_id_ = function_id;
        }

        inline const std::string& data() const {
            return data_;
        }

        inline void set_data(const std::string& data) {
            data_ = data;
        }

    private:
        void* worker_ = nullptr;
        int64_t function_id_ = 0;
        std::string data_;
    };

    class BrokerQueue {
    public:
        BrokerQueue();
        ~BrokerQueue();

        void SetIdleSleepNS(int64_t ns);
        int64_t Size() const;
        bool Empty() const;
        void Push(void* worker, const int64_t& function_id, const std::string& data);
        BrokerMsg* Pop();
        BrokerMsg* TryPop();

    private:
        class BrokerQueueImpl;
        BrokerQueueImpl* m_ = nullptr;
    };

}