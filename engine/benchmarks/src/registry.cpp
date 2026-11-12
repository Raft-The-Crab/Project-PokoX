#include "poko/benchmarks/registry.h"

#include <cstddef>
#include <utility>

namespace poko::benchmarks {

bool BenchmarkRegistry::live(BenchmarkHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < slots_.size() &&
           generations_[handle.index] == handle.generation && slots_[handle.index].alive;
}

Result<BenchmarkHandle> BenchmarkRegistry::add(BenchmarkCase bench) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!valid(bench)) {
        return Result<BenchmarkHandle>::fail(ErrorCode::InvalidArgument,
                                             "benchmark name and body are required");
    }
    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        slots_[index] = Slot{};
    } else {
        index = static_cast<HandleIndex>(slots_.size());
        slots_.push_back(Slot{});
        generations_.push_back(0);
    }
    auto& slot = slots_[index];
    slot.alive = true;
    slot.bench = std::move(bench);
    return Result<BenchmarkHandle>::success(make_handle(index, generations_[index]));
}

Status BenchmarkRegistry::remove(BenchmarkHandle handle) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(handle)) {
        return Status::fail(ErrorCode::NotFound, "benchmark handle is not live");
    }
    slots_[handle.index].alive = false;
    ++generations_[handle.index];
    free_.push_back(handle.index);
    return Status::success();
}

Result<BenchmarkCase> BenchmarkRegistry::get(BenchmarkHandle handle) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(handle)) {
        return Result<BenchmarkCase>::fail(ErrorCode::NotFound,
                                           "benchmark handle is not live");
    }
    return Result<BenchmarkCase>::success(slots_[handle.index].bench);
}

std::vector<BenchmarkCase> BenchmarkRegistry::cases() const {
    std::lock_guard<std::mutex> guard(mutex_);
    std::vector<BenchmarkCase> out;
    out.reserve(slots_.size());
    for (const auto& slot : slots_) {
        if (slot.alive) {
            out.push_back(slot.bench);
        }
    }
    return out;
}

std::vector<BenchmarkHandle> BenchmarkRegistry::handles() const {
    std::lock_guard<std::mutex> guard(mutex_);
    std::vector<BenchmarkHandle> out;
    out.reserve(slots_.size());
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (slots_[i].alive) {
            out.push_back(make_handle(static_cast<HandleIndex>(i), generations_[i]));
        }
    }
    return out;
}

std::uint32_t BenchmarkRegistry::count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    std::uint32_t n = 0;
    for (const auto& slot : slots_) {
        if (slot.alive) {
            ++n;
        }
    }
    return n;
}

} // namespace poko::benchmarks
