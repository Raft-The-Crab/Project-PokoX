#include "poko_frame_stats.h"

#include "poko_log.h"

namespace poko {

void FrameStats::begin_frame() {
    watch_.restart();
    draw_calls_ = 0;
    triangles_ = 0;
    cpu_ms_ = 0.0;
    gpu_ms_ = 0.0;
}

void FrameStats::record_draw(std::uint32_t triangles) noexcept {
    ++draw_calls_;
    triangles_ += triangles;
}

void FrameStats::end_frame() {
    cpu_ms_ = watch_.elapsed_milliseconds();
    ++frame_index_;
}

void FrameStats::reset() noexcept {
    frame_index_ = 0;
    draw_calls_ = 0;
    triangles_ = 0;
    cpu_ms_ = 0.0;
    gpu_ms_ = 0.0;
}

#if defined(POKO_DEV_TOOLS)
void FrameStats::dump_to_log() const {
    POKO_LOG_DEBUG("renderer", "frame=" << frame_index_ << " draws=" << draw_calls_
                                        << " triangles=" << triangles_ << " cpu_ms=" << cpu_ms_
                                        << " gpu_ms=" << gpu_ms_);
}
#endif

} // namespace poko
