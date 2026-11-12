#include "poko_test_harness.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include "poko/benchmarks/runner.h"

namespace {
using poko::TimestampUs;
using poko::benchmarks::BenchmarkCase;
using poko::benchmarks::BenchmarkHandle;
using poko::benchmarks::BenchmarkRegistry;
using poko::benchmarks::BenchmarkRunner;
using poko::benchmarks::ClockNow;

bool near(double a, double b) noexcept {
    return std::abs(a - b) < 1e-9;
}
} // namespace

POKO_TEST(runner_reports_deterministic_stats) {
    BenchmarkRegistry registry;
    std::uint32_t calls = 0;
    auto handle = registry.add(BenchmarkCase{"busy", [&calls]() { ++calls; }});
    POKO_CHECK(handle.ok());

    // Each now() call advances by `step`; the runner calls it twice per
    // iteration, so every iteration measures exactly `step` microseconds.
    TimestampUs value = 0;
    const TimestampUs step = 5;
    ClockNow clock = [&value, step]() -> TimestampUs {
        const TimestampUs current = value;
        value += step;
        return current;
    };

    BenchmarkRunner runner(registry, clock);
    auto result = runner.run(handle.value(), 4);
    POKO_CHECK(result.ok());
    POKO_CHECK_EQ(result.value().name, std::string("busy"));
    POKO_CHECK_EQ(result.value().iterations, 4u);
    POKO_CHECK_EQ(result.value().total_us, TimestampUs{20});
    POKO_CHECK_EQ(result.value().min_us, TimestampUs{5});
    POKO_CHECK_EQ(result.value().max_us, TimestampUs{5});
    POKO_CHECK(near(result.value().mean_us, 5.0));
    POKO_CHECK(near(result.value().ops_per_second, 200000.0));
    POKO_CHECK_EQ(calls, 4u); // body invoked once per iteration
}

POKO_TEST(run_all_reports_each_live_case) {
    BenchmarkRegistry registry;
    auto alpha = registry.add(BenchmarkCase{"alpha", []() {}});
    auto beta = registry.add(BenchmarkCase{"beta", []() {}});
    POKO_CHECK(alpha.ok() && beta.ok());

    TimestampUs value = 0;
    ClockNow clock = [&value]() -> TimestampUs {
        const TimestampUs current = value;
        value += 1;
        return current;
    };

    BenchmarkRunner runner(registry, clock);
    auto results = runner.run_all(3);
    POKO_CHECK_EQ(results.size(), std::size_t{2});
    POKO_CHECK_EQ(results[0].name, std::string("alpha"));
    POKO_CHECK_EQ(results[1].name, std::string("beta"));
    POKO_CHECK_EQ(results[0].total_us, TimestampUs{3});
    POKO_CHECK_EQ(results[1].total_us, TimestampUs{3});
}

POKO_TEST(runner_validates_iterations_handles_and_clock) {
    BenchmarkRegistry registry;
    auto handle = registry.add(BenchmarkCase{"x", []() {}});
    POKO_CHECK(handle.ok());

    BenchmarkRunner runner(registry); // default real clock, never invoked below
    POKO_CHECK(!runner.run(handle.value(), 0).ok());      // zero iterations
    POKO_CHECK(!runner.run(BenchmarkHandle{}, 4).ok());  // unknown handle

    BenchmarkRunner no_clock(registry, ClockNow{}); // empty clock
    POKO_CHECK(!no_clock.run(handle.value(), 4).ok());
}

POKO_TEST(default_clock_is_monotonic) {
    const TimestampUs t0 = poko::benchmarks::steady_now_us();
    const TimestampUs t1 = poko::benchmarks::steady_now_us();
    POKO_CHECK(t1 >= t0);
    POKO_CHECK(t1 > 0);
}

POKO_TEST_MAIN()
