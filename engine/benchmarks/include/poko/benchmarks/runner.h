#pragma once

#include <cstdint>
#include <vector>

#include "poko/benchmarks/benchmark.h"
#include "poko/benchmarks/registry.h"
#include "poko/core/result.h"
#include "poko/core/types.h"

namespace poko::benchmarks {

// Runs registered cases for a fixed iteration count and reports per-iteration
// timing statistics. Timing uses an injected ClockNow so results are
// deterministic in tests; steady_now_us() is the default.
class BenchmarkRunner {
public:
    explicit BenchmarkRunner(const BenchmarkRegistry& registry,
                             ClockNow now = steady_now_us);

    Result<BenchmarkResult> run(BenchmarkHandle handle, std::uint32_t iterations) const;
    std::vector<BenchmarkResult> run_all(std::uint32_t iterations) const;

private:
    const BenchmarkRegistry& registry_;
    ClockNow now_;
};

} // namespace poko::benchmarks
