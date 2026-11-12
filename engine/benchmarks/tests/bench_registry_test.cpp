#include "poko_test_harness.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "poko/benchmarks/registry.h"

namespace {
using poko::benchmarks::BenchmarkCase;
using poko::benchmarks::BenchmarkHandle;
using poko::benchmarks::BenchmarkRegistry;
} // namespace

POKO_TEST(registry_rejects_invalid_cases) {
    BenchmarkRegistry registry;
    POKO_CHECK(!registry.add(BenchmarkCase{}).ok());
    BenchmarkCase no_body;
    no_body.name = "named-but-empty";
    POKO_CHECK(!registry.add(no_body).ok());
    POKO_CHECK_EQ(registry.count(), 0u);
}

POKO_TEST(registry_lifecycle_and_snapshot_order) {
    BenchmarkRegistry registry;
    auto first = registry.add(BenchmarkCase{"first", []() {}});
    auto second = registry.add(BenchmarkCase{"second", []() {}});
    POKO_CHECK(first.ok() && second.ok());
    POKO_CHECK_EQ(registry.count(), 2u);

    auto cases = registry.cases();
    POKO_CHECK_EQ(cases.size(), std::size_t{2});
    POKO_CHECK_EQ(cases[0].name, std::string("first"));
    POKO_CHECK_EQ(cases[1].name, std::string("second"));

    auto got = registry.get(first.value());
    POKO_CHECK(got.ok());
    POKO_CHECK_EQ(got.value().name, std::string("first"));

    POKO_CHECK(registry.remove(first.value()).ok());
    POKO_CHECK(!registry.get(first.value()).ok());
    POKO_CHECK(!registry.remove(first.value()).ok()); // stale handle
    POKO_CHECK_EQ(registry.count(), 1u);

    auto handles = registry.handles();
    POKO_CHECK_EQ(handles.size(), std::size_t{1});
    POKO_CHECK(poko::same_handle(handles[0], second.value()));

    // Freed slot is reused with a bumped generation; snapshot stays slot-ordered.
    auto third = registry.add(BenchmarkCase{"third", []() {}});
    POKO_CHECK(third.ok());
    POKO_CHECK(!poko::same_handle(first.value(), third.value()));
    auto ordered = registry.cases();
    POKO_CHECK_EQ(ordered.size(), std::size_t{2});
    POKO_CHECK_EQ(ordered[0].name, std::string("third"));
    POKO_CHECK_EQ(ordered[1].name, std::string("second"));
}

POKO_TEST_MAIN()
