#pragma once

#include <cstdint>
#include <limits>

namespace poko {

// Stable serialized IDs (systemplan/03). Display names never become IDs.
using EntityId = std::uint64_t;
using HandleIndex = std::uint32_t;
using Generation = std::uint32_t;
using TimestampUs = std::int64_t;

inline constexpr EntityId kInvalidEntityId = 0;
inline constexpr HandleIndex kInvalidHandleIndex = std::numeric_limits<HandleIndex>::max();

struct TypedHandle {
    HandleIndex index = kInvalidHandleIndex;
    Generation generation = 0;
};

constexpr bool is_valid(EntityId id) noexcept {
    return id != kInvalidEntityId;
}

constexpr bool is_valid(const TypedHandle& handle) noexcept {
    return handle.index != kInvalidHandleIndex;
}

constexpr TypedHandle make_handle(HandleIndex index, Generation generation = 0) noexcept {
    return TypedHandle{index, generation};
}

constexpr bool same_handle(const TypedHandle& a, const TypedHandle& b) noexcept {
    return is_valid(a) && a.index == b.index && a.generation == b.generation;
}

static_assert(sizeof(EntityId) == 8, "stable IDs require a 64-bit EntityId");
static_assert(sizeof(TimestampUs) == 8, "timestamps require 64-bit microseconds");

} // namespace poko
