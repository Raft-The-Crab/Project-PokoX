#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "poko_light_contracts.h"
#include "poko_result.h"

namespace poko::lighting {

// Scene light list skeleton (systemplan/04): owns light handles + descriptors
// and exposes a stable per-frame snapshot the renderer consumes. Forward
// lighting only for now; clustered/deferred come later.
class LightList {
public:
    LightList() = default;

    Result<LightHandle> add(const LightDesc& desc);
    Status remove(LightHandle light);
    Status set_desc(LightHandle light, const LightDesc& desc);
    Result<LightDesc> get(LightHandle light) const;

    void set_ambient(const AmbientLight& ambient) noexcept;
    AmbientLight ambient() const noexcept;

    // Per-frame snapshot for upload; order is stable (insertion order).
    struct FrameLight {
        LightKind kind;
        LightDesc desc;
    };
    std::vector<FrameLight> frame_lights() const;

    std::uint32_t count() const noexcept;

private:
    struct Slot {
        bool alive = false;
        LightDesc desc;
    };

    bool live(LightHandle handle) const noexcept; // caller holds mutex_

    mutable std::mutex mutex_;
    AmbientLight ambient_{};
    std::vector<Slot> slots_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
};

} // namespace poko::lighting
