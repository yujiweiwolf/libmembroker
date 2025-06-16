// Copyright 2021 Fancapital Inc.  All rights reserved.
#pragma once
#include <vector>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <memory>

#ifdef _WIN32
#define LOG_INFO __info
#define LOG_ERROR __error
#define LOG_DEBUG __debug
#endif

inline int64_t EncodePrice(double price) {
    return price >= 0 ? static_cast<int64_t> (price * 10000 + 0.5) : static_cast<int64_t> (price * 10000 - 0.5);
}

inline double DecodePrice(int64_t price) {
    return static_cast<double> (price) / 10000;
}
