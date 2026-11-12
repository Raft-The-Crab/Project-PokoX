#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "poko/core/result.h"
#include "poko/core/types.h"

namespace poko::benchmarks {

using BenchmarkHandle = TypedHandle;

// Injected clock returning monotonic microseconds. Letting callers supply it
// makes benchmark runs deterministic in unit tests.
using ClockNow = std::function<TimestampUs()>;

// Default monotonic clock, in microseconds. Declared here, defined in
// src/clock.cpp; tests inject their own ClockNow instead.
TimestampUs steady_now_us();

// A named benchmark body. The body is invoked once per iteration.
struct BenchmarkCase {
    std::string name;
    std::function<void()> body;
};

// Aggregate timing for one run of one case.
struct BenchmarkResult {
    std::string name;
    std::uint32_t iterations = 0;
    TimestampUs total_us = 0;
    TimestampUs min_us = 0;
    TimestampUs max_us = 0;
    double mean_us = 0.0;
    double ops_per_second = 0.0;
};

inline bool valid(const BenchmarkCase& bench) noexcept {
    return !bench.name.empty() && static_cast<bool>(bench.body);
}

} // namespace poko::benchmarks
