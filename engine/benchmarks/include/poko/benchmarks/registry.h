#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "poko/benchmarks/benchmark.h"
#include "poko/core/result.h"
#include "poko/core/types.h"

namespace poko::benchmarks {

// Registry of named benchmark cases with generational handles and a stable
// slot-ordered snapshot for the runner.
class BenchmarkRegistry {
public:
    BenchmarkRegistry() = default;

    Result<BenchmarkHandle> add(BenchmarkCase bench);
    Status remove(BenchmarkHandle handle);
    Result<BenchmarkCase> get(BenchmarkHandle handle) const;

    // Snapshot of live cases, in slot order.
    std::vector<BenchmarkCase> cases() const;
    // Live handles, in slot order (for the runner).
    std::vector<BenchmarkHandle> handles() const;
    std::uint32_t count() const noexcept;

private:
    struct Slot {
        bool alive = false;
        BenchmarkCase bench;
    };
    bool live(BenchmarkHandle handle) const noexcept; // caller holds mutex_

    mutable std::mutex mutex_;
    std::vector<Slot> slots_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
};

} // namespace poko::benchmarks
