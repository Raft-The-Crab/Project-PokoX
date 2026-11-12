#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "poko/core/render_contracts.h"

namespace poko {

// CPU-side scene submission and batching (systemplan/04 step 8). Draws are
// sorted by pipeline then sort_key, and adjacent identical draws are merged
// into instances. No GPU work happens here.
//
// Thread-safety contract: not synchronised; owned by one renderer and used
// from the frame thread only.
class SceneSubmission {
public:
    void submit(const DrawCall& call);
    void clear() noexcept;
    std::size_t size() const noexcept {
        return draws_.size();
    }

    // Stable sort + merge. Idempotent.
    void batch();

    const std::vector<DrawCall>& draws() const noexcept {
        return draws_;
    }
    std::size_t batch_count() const noexcept {
        return batches_;
    }
    std::uint64_t total_triangles() const;

private:
    static bool same_batch(const DrawCall& a, const DrawCall& b) noexcept;

    std::vector<DrawCall> draws_;
    std::size_t batches_ = 0;
};

} // namespace poko
