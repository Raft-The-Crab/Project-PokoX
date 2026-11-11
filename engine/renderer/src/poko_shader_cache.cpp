#include "poko_shader_cache.h"

#include <string_view>
#include <utility>

namespace poko {
namespace {

constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

void mix(std::uint64_t& hash, std::string_view text) noexcept {
    for (const char character : text) {
        hash ^= static_cast<std::uint64_t>(
            static_cast<unsigned char>(character));
        hash *= kFnvPrime;
    }
}

void mix_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= static_cast<std::uint64_t>(value);
    hash *= kFnvPrime;
}

}  // namespace

std::uint64_t ShaderCache::hash(const ShaderDesc& desc) noexcept {
    std::uint64_t value = kFnvOffsetBasis;
    mix_byte(value, static_cast<std::uint8_t>(desc.stage));
    mix(value, desc.entry_point);
    mix_byte(value, 0);  // separator
    mix(value, desc.source);
    return value;
}

ShaderCache::Entry ShaderCache::get_or_compile(const ShaderDesc& desc) {
    const std::uint64_t key = hash(desc);
    std::lock_guard<std::mutex> guard(mutex_);
    const auto found = entries_.find(key);
    if (found != entries_.end()) {
        ++hits_;
        return found->second;
    }

    Entry entry;
    entry.key = key;
    entry.stage = desc.stage;
    entry.entry_point = std::string(desc.entry_point);
    entry.source_bytes = desc.source.size();
    entry.artifact = "mock-artifact:" + std::to_string(key);
    entries_.emplace(key, entry);
    ++compiles_;
    return entry;
}

bool ShaderCache::contains(std::uint64_t key) const {
    std::lock_guard<std::mutex> guard(mutex_);
    return entries_.find(key) != entries_.end();
}

std::size_t ShaderCache::size() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return entries_.size();
}

std::size_t ShaderCache::compile_count() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return compiles_;
}

std::size_t ShaderCache::hit_count() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return hits_;
}

void ShaderCache::clear() {
    std::lock_guard<std::mutex> guard(mutex_);
    entries_.clear();
    compiles_ = 0;
    hits_ = 0;
}

}  // namespace poko
