#include "poko/benchmarks/benchmark.h"

#include <chrono>

namespace poko::benchmarks {

TimestampUs steady_now_us() {
    return static_cast<TimestampUs>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

} // namespace poko::benchmarks
