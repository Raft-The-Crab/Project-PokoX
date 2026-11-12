#pragma once

#include <mutex>
#include <vector>

#include "poko/core/postprocess_contracts.h"
#include "poko/core/result.h"

namespace poko::postprocess {

// Ordered post-process chain skeleton (systemplan/04): owns effect slots in
// render order and exposes a stable per-frame snapshot. The renderer backend
// consumes the snapshot; this class does not touch GPU resources.
class PostProcessChain {
public:
    PostProcessChain() = default;

    Result<PostProcessHandle> add(const PostProcessDesc& desc);
    Status remove(PostProcessHandle handle);
    Status set_desc(PostProcessHandle handle, const PostProcessDesc& desc);
    Result<PostProcessDesc> get(PostProcessHandle handle) const;

    // Snapshot in chain order for upload/execution.
    std::vector<PostProcessDesc> frame_chain() const;

    std::uint32_t count() const noexcept;

private:
    struct Slot {
        bool alive = false;
        PostProcessDesc desc;
    };

    bool live(PostProcessHandle handle) const noexcept;

    mutable std::mutex mutex_;
    std::vector<Slot> slots_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
};

} // namespace poko::postprocess
