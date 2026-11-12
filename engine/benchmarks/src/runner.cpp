#include "poko/benchmarks/runner.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace poko::benchmarks {

BenchmarkRunner::BenchmarkRunner(const BenchmarkRegistry& registry, ClockNow now)
    : registry_(registry), now_(std::move(now)) {}

Result<BenchmarkResult> BenchmarkRunner::run(BenchmarkHandle handle,
                                             std::uint32_t iterations) const {
    if (iterations == 0 || !now_) {
        return Result<BenchmarkResult>::fail(ErrorCode::InvalidArgument,
                                             "iterations must be > 0 and a clock is required");
    }
    auto case_result = registry_.get(handle);
    if (!case_result.ok()) {
        return Result<BenchmarkResult>::fail(case_result.error().code,
                                             case_result.error().message);
    }
    const BenchmarkCase& bench = case_result.value();

    BenchmarkResult result;
    result.name = bench.name;
    result.iterations = iterations;
    result.min_us = std::numeric_limits<TimestampUs>::max();

    TimestampUs total = 0;
    for (std::uint32_t i = 0; i < iterations; ++i) {
        const TimestampUs start = now_();
        bench.body();
        const TimestampUs end = now_();
        const TimestampUs elapsed = end - start;
        if (elapsed < 0) {
            return Result<BenchmarkResult>::fail(ErrorCode::FailedPrecondition,
                                                 "clock moved backwards");
        }
        total += elapsed;
        result.min_us = std::min(result.min_us, elapsed);
        result.max_us = std::max(result.max_us, elapsed);
    }

    result.total_us = total;
    result.mean_us = static_cast<double>(total) / static_cast<double>(iterations);
    result.ops_per_second =
        total > 0
            ? static_cast<double>(iterations) * 1'000'000.0 / static_cast<double>(total)
            : 0.0;
    return Result<BenchmarkResult>::success(std::move(result));
}

std::vector<BenchmarkResult> BenchmarkRunner::run_all(std::uint32_t iterations) const {
    std::vector<BenchmarkResult> results;
    const auto handles = registry_.handles();
    results.reserve(handles.size());
    for (const auto handle : handles) {
        auto result = run(handle, iterations);
        if (result.ok()) {
            results.push_back(result.value());
        }
    }
    return results;
}

} // namespace poko::benchmarks
