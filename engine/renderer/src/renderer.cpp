#include "poko/renderer/renderer.h"

#include <utility>

namespace poko {

Renderer::Renderer(std::unique_ptr<api::IRenderDevice> device) : device_(std::move(device)) {}

std::uint32_t Renderer::triangle_count(const DrawCall& call) noexcept {
    const std::uint32_t indices = call.index_count > 0 ? call.index_count : call.vertex_count;
    return (indices / 3U) * call.instance_count;
}

Status Renderer::render_frame(const ClearValue& clear) {
    if (!device_ || !device_->available()) {
        return Status::fail(ErrorCode::FailedPrecondition, "render device is unavailable");
    }

    stats_.begin_frame();
    Status status = device_->begin_frame();
    if (!status.ok()) {
        return status;
    }

    status = device_->clear(clear);
    if (!status.ok()) {
        (void)device_->end_frame();
        return status;
    }

    for (const auto& call : submission_.draws()) {
        status = device_->draw(call);
        if (!status.ok()) {
            (void)device_->end_frame();
            return status;
        }
        stats_.record_draw(triangle_count(call));
    }

    status = device_->end_frame();
    stats_.end_frame();
    return status;
}

} // namespace poko
