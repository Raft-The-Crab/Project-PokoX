# benchmarks

Deterministic micro-benchmark harness skeleton (systemplan/04 step 9 GPU
timing/diagnostics, systemplan/26 performance gates). Named cases are
registered in a `BenchmarkRegistry` (generational handles) and run for a fixed
iteration count by a `BenchmarkRunner`, which reports total/min/max/mean
microseconds and ops/sec. Timing uses an injectable `ClockNow` so unit tests are
fully deterministic; `steady_now_us()` is the default monotonic clock. No
graphics or vendor dependency, so it builds in every configuration.

Layout:
- `include/poko/benchmarks/benchmark.h` — shared types (`BenchmarkCase`,
  `BenchmarkResult`, `ClockNow`, handle alias) and `steady_now_us()`.
- `include/poko/benchmarks/registry.h` — `BenchmarkRegistry`.
- `include/poko/benchmarks/runner.h` — `BenchmarkRunner`.
- `src/clock.cpp`, `src/registry.cpp`, `src/runner.cpp` — one TU per feature.
- `tests/` — module-local unit tests with an injected step clock.
