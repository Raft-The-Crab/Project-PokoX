#include "poko_render_device.h"

#include <utility>

#include "poko_types.h"

namespace poko {

NullRenderDevice::NullRenderDevice() : capabilities_(null_device_capabilities()) {}

NullRenderDevice::NullRenderDevice(DeviceCapabilities capabilities) : capabilities_(capabilities) {}

const char* NullRenderDevice::name() const noexcept {
    return "null-mock";
}

GraphicsBackend NullRenderDevice::backend() const noexcept {
    return GraphicsBackend::Null;
}

DeviceCapabilities NullRenderDevice::capabilities() const noexcept {
    return capabilities_;
}

bool NullRenderDevice::available() const noexcept {
    return !device_lost_.load(std::memory_order_relaxed);
}

bool NullRenderDevice::handle_live(RenderHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < resources_.size() &&
           generations_[handle.index] == handle.generation && resources_[handle.index].alive;
}

Result<RenderHandle> NullRenderDevice::create(ResourceKind kind, std::uint64_t size_bytes) {
    std::lock_guard<std::mutex> guard(mutex_);
    return create_unlocked(kind, size_bytes);
}

Result<RenderHandle> NullRenderDevice::create_unlocked(ResourceKind kind,
                                                       std::uint64_t size_bytes) {
    if (device_lost_.load(std::memory_order_relaxed)) {
        return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition, "device is lost");
    }
    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        resources_[index] = Resource{kind, true, size_bytes};
    } else {
        index = static_cast<HandleIndex>(resources_.size());
        resources_.push_back(Resource{kind, true, size_bytes});
        generations_.push_back(0);
    }
    return Result<RenderHandle>::success(make_handle(index, generations_[index]));
}

Result<RenderHandle> NullRenderDevice::create_buffer(const BufferDesc& desc) {
    if (!valid(desc)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "invalid buffer descriptor");
    }
    return create(ResourceKind::Buffer, desc.size_bytes);
}

Result<RenderHandle> NullRenderDevice::create_texture(const TextureDesc& desc) {
    if (!valid(desc)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "invalid texture descriptor");
    }
    return create(ResourceKind::Texture);
}

Result<RenderHandle> NullRenderDevice::create_shader(const ShaderDesc& desc) {
    if (!valid(desc)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "invalid shader descriptor");
    }
    return create(ResourceKind::Shader);
}

Result<RenderHandle> NullRenderDevice::create_pipeline(const PipelineDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!valid(desc.layout)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "invalid vertex layout");
    }
    if (!handle_live(desc.vertex_shader) || !handle_live(desc.fragment_shader)) {
        return Result<RenderHandle>::fail(ErrorCode::NotFound, "shader handles are not live");
    }
    return create_unlocked(ResourceKind::Pipeline);
}

Status NullRenderDevice::destroy(RenderHandle handle) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(handle)) {
        return Status::fail(ErrorCode::NotFound, "handle is not live");
    }
    resources_[handle.index].alive = false;
    ++generations_[handle.index];
    free_.push_back(handle.index);
    return Status::success();
}

Status NullRenderDevice::require_frame() const {
    if (device_lost_.load(std::memory_order_relaxed)) {
        return Status::fail(ErrorCode::FailedPrecondition, "device is lost");
    }
    if (!in_frame_) {
        return Status::fail(ErrorCode::FailedPrecondition, "no frame in progress");
    }
    return Status::success();
}

Status NullRenderDevice::update_buffer(RenderHandle handle, std::span<const std::byte> data,
                                       std::uint64_t offset_bytes) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (device_lost_.load(std::memory_order_relaxed)) {
        return Status::fail(ErrorCode::FailedPrecondition, "device is lost");
    }
    if (!handle_live(handle)) {
        return Status::fail(ErrorCode::NotFound, "buffer handle is not live");
    }
    const auto& resource = resources_[handle.index];
    if (resource.kind != ResourceKind::Buffer) {
        return Status::fail(ErrorCode::InvalidArgument, "handle is not a buffer");
    }
    if (data.empty() || offset_bytes > resource.size_bytes ||
        data.size() > resource.size_bytes - offset_bytes) {
        return Status::fail(ErrorCode::OutOfRange, "buffer update is out of range");
    }
    ++buffer_updates_;
    return Status::success();
}

Status NullRenderDevice::begin_frame() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (device_lost_.load(std::memory_order_relaxed)) {
        return Status::fail(ErrorCode::FailedPrecondition, "device is lost");
    }
    if (in_frame_) {
        return Status::fail(ErrorCode::AlreadyExists, "frame already in progress");
    }
    in_frame_ = true;
    ++frame_count_;
    return Status::success();
}

Status NullRenderDevice::clear(const ClearValue& value) {
    std::lock_guard<std::mutex> guard(mutex_);
    const Status status = require_frame();
    if (!status.ok()) {
        return status;
    }
    clears_.push_back(value);
    return Status::success();
}

Status NullRenderDevice::draw(const DrawCall& call) {
    std::lock_guard<std::mutex> guard(mutex_);
    const Status status = require_frame();
    if (!status.ok()) {
        return status;
    }
    if (!valid(call)) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid draw call");
    }
    if (!handle_live(call.pipeline)) {
        return Status::fail(ErrorCode::NotFound, "pipeline handle is not live");
    }
    draws_.push_back(call);
    return Status::success();
}

Status NullRenderDevice::end_frame() {
    std::lock_guard<std::mutex> guard(mutex_);
    const Status status = require_frame();
    if (!status.ok()) {
        return status;
    }
    in_frame_ = false;
    return Status::success();
}

void NullRenderDevice::wait_idle() {
    std::lock_guard<std::mutex> guard(mutex_);
    // A MOCK device has no GPU work in flight.
}

bool NullRenderDevice::device_lost() const noexcept {
    return device_lost_.load(std::memory_order_relaxed);
}

Status NullRenderDevice::recover_from_device_loss() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!device_lost_.load(std::memory_order_relaxed)) {
        return Status::fail(ErrorCode::FailedPrecondition, "device is not lost");
    }
    // In-flight work is discarded; handles survive but commands are reset.
    device_lost_.store(false, std::memory_order_relaxed);
    in_frame_ = false;
    clears_.clear();
    draws_.clear();
    return Status::success();
}

void NullRenderDevice::simulate_device_loss() noexcept {
    device_lost_.store(true, std::memory_order_relaxed);
}

std::size_t NullRenderDevice::resource_count() const {
    std::lock_guard<std::mutex> guard(mutex_);
    std::size_t live = 0;
    for (const auto& resource : resources_) {
        if (resource.alive) {
            ++live;
        }
    }
    return live;
}

std::size_t NullRenderDevice::frame_count() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return frame_count_;
}

std::size_t NullRenderDevice::buffer_update_count() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return buffer_updates_;
}

std::vector<ClearValue> NullRenderDevice::recorded_clears() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return clears_;
}

std::vector<DrawCall> NullRenderDevice::recorded_draws() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return draws_;
}

void NullRenderDevice::reset_records() {
    std::lock_guard<std::mutex> guard(mutex_);
    clears_.clear();
    draws_.clear();
    frame_count_ = 0;
}

bool NullRenderDevice::is_handle_live(RenderHandle handle) const {
    std::lock_guard<std::mutex> guard(mutex_);
    return handle_live(handle);
}

GraphicsBackend preferred_backend() noexcept {
    return GraphicsBackend::Vulkan;
}

GraphicsBackend fallback_backend() noexcept {
    return GraphicsBackend::OpenGL;
}

GraphicsBackend last_resort_backend() noexcept {
    return GraphicsBackend::Null;
}

DeviceCapabilities null_device_capabilities() noexcept {
    DeviceCapabilities capabilities;
    capabilities.backend = GraphicsBackend::Null;
    capabilities.device_name = "null-mock";
    capabilities.max_texture_size = 16384;
    capabilities.max_buffer_bytes = 1ULL << 30;
    capabilities.supports_compute = false;
    capabilities.supports_wireframe = false;
    capabilities.supports_shader_reload = true;
    return capabilities;
}

} // namespace poko
