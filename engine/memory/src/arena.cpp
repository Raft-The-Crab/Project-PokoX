#include "poko/memory/allocators.h"

#include <cstdlib>

namespace poko::memory {
namespace {

std::size_t align_up(std::size_t value, std::size_t alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace

Arena::Arena(std::size_t capacity_bytes)
    : owned_(new std::uint8_t[capacity_bytes]), base_(owned_.get()), capacity_(capacity_bytes) {}

Arena::~Arena() = default;

void* Arena::allocate(std::size_t size, std::size_t alignment) {
    const std::size_t aligned = align_up(offset_, alignment);
    if (aligned > capacity_ || size > capacity_ - aligned) {
        return nullptr;
    }
    void* ptr = base_ + aligned;
    offset_ = aligned + size;
    return ptr;
}

} // namespace poko::memory
