#include <algorithm>
#include <cstdio>
#include <cstring>

#include "sw_detail.h"

namespace poko {

SoftwareRenderDevice::SoftwareRenderDevice(std::uint32_t width, std::uint32_t height)
    : width_(std::max(width, 1u)),
      height_(std::max(height, 1u)),
      color_(static_cast<std::size_t>(width_) * height_, 0xFF000000u),
      depth_(static_cast<std::size_t>(width_) * height_, 1.0f) {}

SoftwareRenderDevice::~SoftwareRenderDevice() = default;

const char* SoftwareRenderDevice::name() const noexcept {
    return "software-rasterizer";
}

GraphicsBackend SoftwareRenderDevice::backend() const noexcept {
    return GraphicsBackend::Software;
}

DeviceCapabilities SoftwareRenderDevice::capabilities() const noexcept {
    DeviceCapabilities caps;
    caps.backend = GraphicsBackend::Software;
    caps.device_name = "software-rasterizer";
    caps.max_texture_size = 8192;
    caps.max_buffer_bytes = 256ull * 1024 * 1024;
    return caps;
}

bool SoftwareRenderDevice::available() const noexcept {
    return true;
}

Result<RenderHandle> SoftwareRenderDevice::add(Slot&& slot) {
    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        slots_[index] = std::move(slot);
    } else {
        index = static_cast<HandleIndex>(slots_.size());
        slots_.push_back(std::move(slot));
        generations_.push_back(0);
    }
    slots_[index].alive = true;
    return Result<RenderHandle>::success(make_handle(index, generations_[index]));
}

const SoftwareRenderDevice::Slot* SoftwareRenderDevice::get(RenderHandle handle) const noexcept {
    if (!is_valid(handle) || static_cast<std::size_t>(handle.index) >= slots_.size() ||
        generations_[handle.index] != handle.generation || !slots_[handle.index].alive) {
        return nullptr;
    }
    return &slots_[handle.index];
}

Result<RenderHandle> SoftwareRenderDevice::create_buffer(const BufferDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (lost_) return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition, "device lost");
    if (!valid(desc) || desc.size_bytes > capabilities().max_buffer_bytes) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "invalid buffer descriptor");
    }
    Slot slot;
    slot.kind = SwKind::Buffer;
    slot.bytes.assign(static_cast<std::size_t>(desc.size_bytes), std::byte{0});
    return add(std::move(slot));
}

Result<RenderHandle> SoftwareRenderDevice::create_texture(const TextureDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (lost_) return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition, "device lost");
    const auto max = capabilities().max_texture_size;
    if (!valid(desc) || desc.width > max || desc.height > max) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "invalid texture descriptor");
    }
    Slot slot;
    slot.kind = SwKind::Texture;
    slot.texture = desc;
    slot.texture.debug_name = {};
    // Opaque magenta until uploaded makes a missing upload obvious.
    const auto bpp = swdetail::bytes_per_pixel(desc.format);
    slot.bytes.assign(static_cast<std::size_t>(desc.width) * desc.height * bpp, std::byte{0});
    if (desc.format == PixelFormat::RGBA8) {
        for (std::size_t i = 0; i + 3 < slot.bytes.size(); i += 4) {
            slot.bytes[i] = std::byte{255};
            slot.bytes[i + 2] = std::byte{255};
            slot.bytes[i + 3] = std::byte{255};
        }
    }
    return add(std::move(slot));
}

Result<RenderHandle> SoftwareRenderDevice::create_shader(const ShaderDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (lost_) return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition, "device lost");
    if (!valid(desc)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "invalid shader descriptor");
    }
    if (desc.stage == ShaderStage::Compute) {
        return Result<RenderHandle>::fail(ErrorCode::Unsupported, "compute shaders are unsupported");
    }
    Slot slot;
    slot.kind = SwKind::Shader;
    slot.stage = desc.stage;
    slot.source = std::string(desc.source);
    return add(std::move(slot));
}

Result<RenderHandle> SoftwareRenderDevice::create_pipeline(const PipelineDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (lost_) return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition, "device lost");
    const Slot* vs = get(desc.vertex_shader);
    const Slot* fs = get(desc.fragment_shader);
    if (vs == nullptr || fs == nullptr || vs->kind != SwKind::Shader ||
        fs->kind != SwKind::Shader || vs->stage != ShaderStage::Vertex ||
        fs->stage != ShaderStage::Fragment || !valid(desc.layout)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "invalid pipeline descriptor");
    }
    Slot slot;
    slot.kind = SwKind::Pipeline;
    slot.pipeline = desc;
    slot.pipeline.debug_name = {};
    const auto& src = fs->source;
    if (src.find("poko:textured") != std::string::npos) {
        slot.program = SwProgram::Textured;
    } else if (src.find("poko:lambert") != std::string::npos) {
        slot.program = SwProgram::Lambert;
    } else if (src.find("poko:vertex_color") != std::string::npos) {
        slot.program = SwProgram::VertexColor;
    }
    return add(std::move(slot));
}

Result<RenderHandle> SoftwareRenderDevice::create_sampler(const SamplerDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (lost_) return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition, "device lost");
    Slot slot;
    slot.kind = SwKind::Sampler;
    slot.sampler = desc;
    slot.sampler.debug_name = {};
    return add(std::move(slot));
}

Status SoftwareRenderDevice::destroy(RenderHandle handle) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (get(handle) == nullptr) {
        return Status::fail(ErrorCode::NotFound, "render handle is not live");
    }
    slots_[handle.index] = Slot{};
    ++generations_[handle.index];
    free_.push_back(handle.index);
    return Status::success();
}

Status SoftwareRenderDevice::update_buffer(RenderHandle handle, std::span<const std::byte> data,
                                           std::uint64_t offset_bytes) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (lost_) return Status::fail(ErrorCode::FailedPrecondition, "device lost");
    const Slot* slot = get(handle);
    if (slot == nullptr || slot->kind != SwKind::Buffer) {
        return Status::fail(ErrorCode::NotFound, "not a live buffer");
    }
    if (data.empty()) return Status::fail(ErrorCode::InvalidArgument, "empty upload");
    if (offset_bytes > slot->bytes.size() || data.size() > slot->bytes.size() - offset_bytes) {
        return Status::fail(ErrorCode::OutOfRange, "upload past the end of the buffer");
    }
    std::memcpy(slots_[handle.index].bytes.data() + offset_bytes, data.data(), data.size());
    return Status::success();
}

Status SoftwareRenderDevice::update_texture(RenderHandle handle, std::uint32_t mip_level,
                                            std::span<const std::byte> data) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (lost_) return Status::fail(ErrorCode::FailedPrecondition, "device lost");
    const Slot* slot = get(handle);
    if (slot == nullptr || slot->kind != SwKind::Texture) {
        return Status::fail(ErrorCode::NotFound, "not a live texture");
    }
    if (mip_level >= slot->texture.mip_levels) {
        return Status::fail(ErrorCode::OutOfRange, "mip level out of range");
    }
    const auto bpp = swdetail::bytes_per_pixel(slot->texture.format);
    const auto w = std::max(1u, slot->texture.width >> mip_level);
    const auto h = std::max(1u, slot->texture.height >> mip_level);
    if (data.size() != static_cast<std::size_t>(w) * h * bpp) {
        return Status::fail(ErrorCode::InvalidArgument, "upload must cover the whole mip level");
    }
    if (mip_level == 0) {
        std::memcpy(slots_[handle.index].bytes.data(), data.data(), data.size());
    }
    return Status::success();
}

Status SoftwareRenderDevice::begin_frame() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (lost_) return Status::fail(ErrorCode::FailedPrecondition, "device lost");
    if (in_frame_) return Status::fail(ErrorCode::FailedPrecondition, "frame already begun");
    in_frame_ = true;
    return Status::success();
}

Status SoftwareRenderDevice::clear(const ClearValue& value) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (lost_) return Status::fail(ErrorCode::FailedPrecondition, "device lost");
    if (!in_frame_) return Status::fail(ErrorCode::FailedPrecondition, "no frame in progress");
    const auto to8 = [](float v) {
        return static_cast<std::uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    const std::uint32_t packed =
        to8(value.r) | (to8(value.g) << 8) | (to8(value.b) << 16) | (to8(value.a) << 24);
    std::fill(color_.begin(), color_.end(), packed);
    std::fill(depth_.begin(), depth_.end(), value.depth);
    return Status::success();
}

Status SoftwareRenderDevice::end_frame() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (lost_) return Status::fail(ErrorCode::FailedPrecondition, "device lost");
    if (!in_frame_) return Status::fail(ErrorCode::FailedPrecondition, "no frame in progress");
    in_frame_ = false;
    ++frames_;
    return Status::success();
}

void SoftwareRenderDevice::wait_idle() {}

bool SoftwareRenderDevice::device_lost() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return lost_;
}

Status SoftwareRenderDevice::recover_from_device_loss() {
    std::lock_guard<std::mutex> guard(mutex_);
    lost_ = false;
    in_frame_ = false;
    return Status::success();
}

void SoftwareRenderDevice::simulate_device_loss() noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    lost_ = true;
    in_frame_ = false;
}

std::vector<std::uint32_t> SoftwareRenderDevice::pixels() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return color_;
}

std::uint32_t SoftwareRenderDevice::pixel(std::uint32_t x, std::uint32_t y) const {
    std::lock_guard<std::mutex> guard(mutex_);
    return x < width_ && y < height_ ? color_[static_cast<std::size_t>(y) * width_ + x] : 0u;
}

float SoftwareRenderDevice::depth_at(std::uint32_t x, std::uint32_t y) const {
    std::lock_guard<std::mutex> guard(mutex_);
    return x < width_ && y < height_ ? depth_[static_cast<std::size_t>(y) * width_ + x] : 1.0f;
}

Status SoftwareRenderDevice::write_ppm(const std::string& path) const {
    std::lock_guard<std::mutex> guard(mutex_);
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) return Status::fail(ErrorCode::NotFound, "cannot open output file");
    std::fprintf(file, "P6\n%u %u\n255\n", width_, height_);
    std::vector<unsigned char> row(static_cast<std::size_t>(width_) * 3);
    for (std::uint32_t y = 0; y < height_; ++y) {
        for (std::uint32_t x = 0; x < width_; ++x) {
            const auto p = color_[static_cast<std::size_t>(y) * width_ + x];
            row[x * 3 + 0] = static_cast<unsigned char>(p & 0xFF);
            row[x * 3 + 1] = static_cast<unsigned char>((p >> 8) & 0xFF);
            row[x * 3 + 2] = static_cast<unsigned char>((p >> 16) & 0xFF);
        }
        std::fwrite(row.data(), 1, row.size(), file);
    }
    const bool ok = std::fclose(file) == 0;
    return ok ? Status::success() : Status::fail(ErrorCode::Internal, "write failed");
}

std::uint64_t SoftwareRenderDevice::triangles_drawn() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return triangles_;
}

std::uint64_t SoftwareRenderDevice::pixels_shaded() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return shaded_;
}

std::uint64_t SoftwareRenderDevice::frame_count() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return frames_;
}

} // namespace poko
