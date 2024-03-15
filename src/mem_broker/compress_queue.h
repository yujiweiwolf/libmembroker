#pragma once
#include <string>

namespace co {

    constexpr int kCompressActionReadPlain = 1;
    constexpr int kCompressActionReadUncompress = 2;
    constexpr int kCompressActionWriteCompress = 3;

    class CompressQueue {
    public:
        CompressQueue();
        ~CompressQueue();

        int64_t Size() const;
        bool Empty() const;
        void Push(int action, const int64_t& type, const std::string& data);
        int64_t Pop(int* action, std::string* data, int64_t timeout_ns = 0, int64_t sleep_ns = 0);

        void Clear();

    private:
        class CompressQueueImpl;
        CompressQueueImpl* m_ = nullptr;
    };

}