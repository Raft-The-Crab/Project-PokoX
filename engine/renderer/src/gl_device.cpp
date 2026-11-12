#include "poko/renderer/gl_device.h"

#include "gl_conversions.h"

namespace poko {

GlRenderDevice::GlRenderDevice() = default;

GlRenderDevice::~GlRenderDevice() {
    if (!initialized_) {
        return;
    }
    for (const auto& resource : resources_) {
        if (!resource.alive) {
            continue;
        }
        switch (resource.kind) {
        case Kind::Buffer:
            glDeleteBuffers(1, &resource.gl_id);
            break;
        case Kind::Texture:
            glDeleteTextures(1, &resource.gl_id);
            break;
        case Kind::Shader:
            glDeleteShader(resource.gl_id);
            break;
        case Kind::Pipeline:
            glDeleteProgram(resource.gl_id);
            break;
        case Kind::Sampler:
            glDeleteSamplers(1, &resource.gl_id);
            break;
        }
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
    }
}

bool GlRenderDevice::initialize() noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    if (initialized_) {
        return true;
    }
    if (gl3wInit() != 0 || !gl3wIsSupported(3, 3)) {
        return false;
    }

    renderer_name_ = gl::gl_string(GL_RENDERER);
    capabilities_.backend = GraphicsBackend::OpenGL;
    capabilities_.device_name = renderer_name_.c_str();
    GLint max_texture_size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    capabilities_.max_texture_size = static_cast<std::uint32_t>(max_texture_size);
    // Desktop GL exposes no direct "maximum buffer size" query; report a
    // conservative floor until a real path exists (noted in DEC-004).
    capabilities_.max_buffer_bytes = std::uint64_t{1} << 30;
    GLint major = 0;
    GLint minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    capabilities_.supports_compute = (major > 4) || (major == 4 && minor >= 3);
    capabilities_.supports_wireframe = true;
    capabilities_.supports_shader_reload = true;

    glGenVertexArrays(1, &vao_);
    if (vao_ == 0) {
        return false;
    }
    initialized_ = true;
    return true;
}

const char* GlRenderDevice::name() const noexcept {
    return "opengl";
}

GraphicsBackend GlRenderDevice::backend() const noexcept {
    return GraphicsBackend::OpenGL;
}

DeviceCapabilities GlRenderDevice::capabilities() const noexcept {
    return capabilities_;
}

bool GlRenderDevice::available() const noexcept {
    return initialized_;
}

bool GlRenderDevice::handle_live(RenderHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < resources_.size() &&
           generations_[handle.index] == handle.generation && resources_[handle.index].alive;
}

GlRenderDevice::Resource* GlRenderDevice::find(HandleIndex index) noexcept {
    if (static_cast<std::size_t>(index) >= resources_.size()) {
        return nullptr;
    }
    return &resources_[index];
}

const GlRenderDevice::Resource* GlRenderDevice::find(HandleIndex index) const noexcept {
    if (static_cast<std::size_t>(index) >= resources_.size()) {
        return nullptr;
    }
    return &resources_[index];
}

Status GlRenderDevice::require_frame() const {
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition, "device is not initialized");
    }
    if (!in_frame_) {
        return Status::fail(ErrorCode::FailedPrecondition, "no frame in progress");
    }
    return Status::success();
}

Status GlRenderDevice::gl_error_status(const char* what) const {
    const GLenum error = glGetError();
    if (error == GL_NO_ERROR) {
        return Status::success();
    }
    return Status::fail(ErrorCode::Internal,
                        std::string(what) + " failed (GL error " + std::to_string(error) + ")");
}

Status GlRenderDevice::begin_frame() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition, "device is not initialized");
    }
    if (in_frame_) {
        return Status::fail(ErrorCode::AlreadyExists, "frame already in progress");
    }
    in_frame_ = true;
    return Status::success();
}

Status GlRenderDevice::end_frame() {
    std::lock_guard<std::mutex> guard(mutex_);
    const Status status = require_frame();
    if (!status.ok()) {
        return status;
    }
    in_frame_ = false;
    glFlush();
    return gl_error_status("end_frame");
}

void GlRenderDevice::wait_idle() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (initialized_) {
        glFinish();
    }
}

bool GlRenderDevice::device_lost() const noexcept {
    return false;
}

Status GlRenderDevice::recover_from_device_loss() {
    return Status::fail(ErrorCode::FailedPrecondition, "device is not lost");
}

void GlRenderDevice::set_viewport(std::uint32_t width, std::uint32_t height) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    if (initialized_) {
        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    }
}

} // namespace poko
