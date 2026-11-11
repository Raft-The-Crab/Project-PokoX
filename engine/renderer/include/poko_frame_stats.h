#pragma once

#include <cstddef>
#include <cstdint>

#include "poko_clock.h"

namespace poko {

// Frame and GPU timing diagnostics (systemplan/04 step 9). The dev-only dump
// is compiled only in Debug/Development so debug tooling cannot leak into
// release or headless server builds (systemplan/00, rule 12 of systemplan/30).
//
// Thread-safety contract: not synchronised; owned by one renderer and used
// from the frame thread only.
class FrameStats {
public:
    void begin_frame();
    void record_draw(std::uint32_t triangles) noexcept;
    void set_gpu_time_ms(double milliseconds) noexcept {
        gpu_ms_ = milliseconds;
    }
    void end_frame();

    std::uint64_t frame_index() const noexcept {
        return frame_index_;
    }
    std::uint32_t draw_calls() const noexcept {
        return draw_calls_;
    }
    std::uint64_t triangles() const noexcept {
        return triangles_;
    }
    double cpu_frame_ms() const noexcept {
        return cpu_ms_;
    }
    double gpu_frame_ms() const noexcept {
        return gpu_ms_;
    }

    void reset() noexcept;

#if defined(POKO_DEV_TOOLS)
    // Development-only console dump; the eventual Dear ImGui panel
    // (systemplan/09) reads the same counters.
    void dump_to_log() const;
#endif

private:
    Stopwatch watch_;
    std::uint64_t frame_index_ = 0;
    std::uint32_t draw_calls_ = 0;
    std::uint64_t triangles_ = 0;
    double cpu_ms_ = 0.0;
    double gpu_ms_ = 0.0;
};

} // namespace poko
