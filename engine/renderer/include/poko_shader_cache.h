#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

#include "poko_render_contracts.h"

namespace poko {

// Shader compilation cache (systemplan/04 step 3). Backend-neutral CPU logic.
// Keys are a stable FNV-1a hash of stage + entry point + source, so a clean
// checkout produces identical artifacts. The artifact is a MOCK placeholder
// until a real shader compiler backend is pinned.
//
// Thread-safety contract: all methods are safe to call concurrently.
class ShaderCache {
public:
    struct Entry {
        std::uint64_t key = 0;
        ShaderStage stage = ShaderStage::Vertex;
        std::string entry_point;
        std::size_t source_bytes = 0;
        std::string artifact;
    };

    static std::uint64_t hash(const ShaderDesc& desc) noexcept;

    Entry get_or_compile(const ShaderDesc& desc);
    bool contains(std::uint64_t key) const;
    std::size_t size() const;
    std::size_t compile_count() const;
    std::size_t hit_count() const;
    void clear();

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, Entry> entries_;
    std::size_t compiles_ = 0;
    std::size_t hits_ = 0;
};

}  // namespace poko
