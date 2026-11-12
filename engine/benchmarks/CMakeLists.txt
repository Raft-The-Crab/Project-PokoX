# Deterministic micro-benchmark harness skeleton (Phase 1). No graphics/vendor
# dependency: builds in every configuration.
add_library(poko_benchmarks STATIC
    src/clock.cpp
    src/registry.cpp
    src/runner.cpp)

target_include_directories(poko_benchmarks PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_benchmarks PUBLIC poko_core)
poko_apply_common(poko_benchmarks)
poko_set_impl_label(poko_benchmarks PRODUCTION)

poko_add_unit_tests(poko_benchmarks
    TEST_SOURCES
        bench_registry_test.cpp
        bench_runner_test.cpp)
