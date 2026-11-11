#include "poko_gl_device.h"

#include <cstdint>
#include <cstring>
#include <string>

#include <GL/gl3w.h>

// windows.h (pulled in by glcorearb.h) #defines DeviceCapabilities to the
// Win32 DEVICE_CAPABILITIES struct name; it collides with poko's type.
#ifdef DeviceCapabilities
#undef DeviceCapabilities
#endif

namespace poko {
namespace {

std::uint32_t buffer_target(BufferUsage usage) noexcept {
    switch (usage) {
    case BufferUsage::Vertex:
        return GL_ARRAY_BUFFER;
    case BufferUsage::Index:
        return GL_ELEMENT_ARRAY_BUFFER;
    case BufferUsage::Uniform:
        return GL_UNIFORM_BUFFER;
    case BufferUsage::Storage:
        return GL_SHADER_STORAGE_BUFFER;
    case BufferUsage::Staging:
        return GL_PIXEL_UNPACK_BUFFER;
    }
    return GL_ARRAY_BUFFER;
}

std::uint32_t buffer_usage_hint(BufferUsage usage) noexcept {
    switch (usage) {
    case BufferUsage::Staging:
        return GL_STREAM_DRAW;
    default:
        return GL_DYNAMIC_DRAW;
    }
}

std::uint32_t shader_stage(ShaderStage stage) noexcept {
    switch (stage) {
    case ShaderStage::Vertex:
        return GL_VERTEX_SHADER;
    case ShaderStage::Fragment:
        return GL_FRAGMENT_SHADER;
    case ShaderStage::Compute:
        return GL_COMPUTE_SHADER;
    }
    return GL_VERTEX_SHADER;
}

std::uint32_t primitive_mode(PrimitiveTopology topology) noexcept {
    switch (topology) {
    case PrimitiveTopology::Triangles:
        return GL_TRIANGLES;
    case PrimitiveTopology::TriangleStrip:
        return GL_TRIANGLE_STRIP;
    case PrimitiveTopology::Lines:
        return GL_LINES;
    case PrimitiveTopology::Points:
        return GL_POINTS;
    }
    return GL_TRIANGLES;
}

std::uint32_t compare_op(CompareOp op) noexcept {
    switch (op) {
    case CompareOp::Never:
        return GL_NEVER;
    case CompareOp::Less:
        return GL_LESS;
    case CompareOp::Equal:
        return GL_EQUAL;
    case CompareOp::LessOrEqual:
        return GL_LEQUAL;
    case CompareOp::Greater:
        return GL_GREATER;
    case CompareOp::NotEqual:
        return GL_NOTEQUAL;
    case CompareOp::GreaterOrEqual:
        return GL_GEQUAL;
    case CompareOp::Always:
        return GL_ALWAYS;
    }
    return GL_LEQUAL;
}

std::uint32_t cull_face(CullMode mode) noexcept {
    switch (mode) {
    case CullMode::None:
        return 0;
    case CullMode::Front:
        return GL_FRONT;
    case CullMode::Back:
        return GL_BACK;
    }
    return GL_BACK;
}

// Vertex attributes are 32-bit float tuples; the seam carries no element-type
// field yet (noted in DEC-004).
constexpr std::uint32_t kAttributeType = GL_FLOAT;

struct GlFormat {
    std::uint32_t internal_format;
    std::uint32_t format;
    std::uint32_t type;
    std::uint32_t channels;
};

bool pixel_format(PixelFormat format, GlFormat& out) noexcept {
    switch (format) {
    case PixelFormat::RGBA8:
        out = {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4};
        return true;
    case PixelFormat::BGRA8:
        out = {GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, 4};
        return true;
    case PixelFormat::RGB8:
        out = {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, 3};
        return true;
    case PixelFormat::R8:
        out = {GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1};
        return true;
    case PixelFormat::RGBA16F:
        out = {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 4};
        return true;
    case PixelFormat::RGBA32F:
        out = {GL_RGBA32F, GL_RGBA, GL_FLOAT, 4};
        return true;
    case PixelFormat::Depth24Stencil8:
        out = {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 2};
        return true;
    case PixelFormat::Depth32F:
        out = {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, 1};
        return true;
    case PixelFormat::Unknown:
        break;
    }
    return false;
}

const char* gl_string(std::uint32_t name) {
    const auto* value = glGetString(name);
    return value != nullptr ? reinterpret_cast<const char*>(value) : "";
}

} // namespace

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

    renderer_name_ = gl_string(GL_RENDERER);
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

Result<RenderHandle> GlRenderDevice::create_buffer(const BufferDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition,
                                          "device is not initialized");
    }
    if (!valid(desc) || desc.size_bytes > (std::uint64_t{1} << 32)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "invalid buffer descriptor");
    }
    GLuint id = 0;
    glGenBuffers(1, &id);
    if (id == 0) {
        return Result<RenderHandle>::fail(ErrorCode::ResourceExhausted, "glGenBuffers failed");
    }
    const std::uint32_t target = buffer_target(desc.usage);
    glBindBuffer(target, id);
    glBufferData(target, static_cast<GLsizeiptr>(desc.size_bytes), nullptr,
                 buffer_usage_hint(desc.usage));
    glBindBuffer(target, 0);
    if (const Status status = gl_error_status("glBufferData"); !status.ok()) {
        glDeleteBuffers(1, &id);
        return Result<RenderHandle>::fail(status.code(), status.error().message);
    }

    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        resources_[index] = Resource{};
    } else {
        index = static_cast<HandleIndex>(resources_.size());
        resources_.push_back(Resource{});
        generations_.push_back(0);
    }
    auto& resource = resources_[index];
    resource.kind = Kind::Buffer;
    resource.alive = true;
    resource.gl_id = id;
    resource.target = target;
    resource.size_bytes = desc.size_bytes;
    return Result<RenderHandle>::success(make_handle(index, generations_[index]));
}

Result<RenderHandle> GlRenderDevice::create_texture(const TextureDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition,
                                          "device is not initialized");
    }
    if (!valid(desc)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "invalid texture descriptor");
    }
    if (desc.depth != 1 || desc.array_layers != 1) {
        return Result<RenderHandle>::fail(
            ErrorCode::Unsupported, "only 2D non-array textures are supported by the GL backend");
    }
    GlFormat format{};
    if (!pixel_format(desc.format, format)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "unsupported pixel format");
    }
    GLuint id = 0;
    glGenTextures(1, &id);
    if (id == 0) {
        return Result<RenderHandle>::fail(ErrorCode::ResourceExhausted, "glGenTextures failed");
    }
    glBindTexture(GL_TEXTURE_2D, id);
    // Immutable-format glTexStorage* needs GL 4.2; allocate per-level with
    // glTexImage2D so the 3.3 core floor stays valid.
    for (std::uint32_t level = 0; level < desc.mip_levels; ++level) {
        const std::uint32_t shift = level < 32 ? level : 31;
        const std::uint32_t width =
            level >= 32 ? 1 : (desc.width >> shift == 0 ? 1 : desc.width >> shift);
        const std::uint32_t height =
            level >= 32 ? 1 : (desc.height >> shift == 0 ? 1 : desc.height >> shift);
        glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(level),
                     static_cast<GLint>(format.internal_format), static_cast<GLsizei>(width),
                     static_cast<GLsizei>(height), 0, format.format, format.type, nullptr);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    desc.mip_levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (const Status status = gl_error_status("glTexImage2D"); !status.ok()) {
        glDeleteTextures(1, &id);
        return Result<RenderHandle>::fail(status.code(), status.error().message);
    }

    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        resources_[index] = Resource{};
    } else {
        index = static_cast<HandleIndex>(resources_.size());
        resources_.push_back(Resource{});
        generations_.push_back(0);
    }
    auto& resource = resources_[index];
    resource.kind = Kind::Texture;
    resource.alive = true;
    resource.gl_id = id;
    return Result<RenderHandle>::success(make_handle(index, generations_[index]));
}

Result<RenderHandle> GlRenderDevice::create_shader(const ShaderDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition,
                                          "device is not initialized");
    }
    if (!valid(desc)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "invalid shader descriptor");
    }
    // GLSL sources must carry their own #version; the entry point is fixed to
    // main() by the language (SPIR-V entry points arrive with Vulkan, DEC-004).
    const GLuint id = glCreateShader(shader_stage(desc.stage));
    if (id == 0) {
        return Result<RenderHandle>::fail(ErrorCode::ResourceExhausted, "glCreateShader failed");
    }
    const char* source = desc.source.data();
    const GLsizei length = static_cast<GLsizei>(desc.source.size());
    glShaderSource(id, 1, &source, &length);
    glCompileShader(id);
    GLint compiled = GL_FALSE;
    glGetShaderiv(id, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLint log_length = 0;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &log_length);
        std::string log(static_cast<std::size_t>(log_length > 0 ? log_length : 1), '\0');
        if (log_length > 1) {
            glGetShaderInfoLog(id, log_length, nullptr, log.data());
        }
        glDeleteShader(id);
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument,
                                          "shader compile failed: " + log);
    }

    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        resources_[index] = Resource{};
    } else {
        index = static_cast<HandleIndex>(resources_.size());
        resources_.push_back(Resource{});
        generations_.push_back(0);
    }
    auto& resource = resources_[index];
    resource.kind = Kind::Shader;
    resource.alive = true;
    resource.gl_id = id;
    return Result<RenderHandle>::success(make_handle(index, generations_[index]));
}

Result<RenderHandle> GlRenderDevice::create_pipeline(const PipelineDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition,
                                          "device is not initialized");
    }
    if (!valid(desc.layout)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "invalid vertex layout");
    }
    const Resource* vertex =
        handle_live(desc.vertex_shader) ? find(desc.vertex_shader.index) : nullptr;
    const Resource* fragment =
        handle_live(desc.fragment_shader) ? find(desc.fragment_shader.index) : nullptr;
    if (vertex == nullptr || fragment == nullptr || vertex->kind != Kind::Shader ||
        fragment->kind != Kind::Shader) {
        return Result<RenderHandle>::fail(ErrorCode::NotFound, "shader handles are not live");
    }

    const GLuint program = glCreateProgram();
    if (program == 0) {
        return Result<RenderHandle>::fail(ErrorCode::ResourceExhausted, "glCreateProgram failed");
    }
    glAttachShader(program, vertex->gl_id);
    glAttachShader(program, fragment->gl_id);
    glLinkProgram(program);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint log_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        std::string log(static_cast<std::size_t>(log_length > 0 ? log_length : 1), '\0');
        if (log_length > 1) {
            glGetProgramInfoLog(program, log_length, nullptr, log.data());
        }
        glDeleteProgram(program);
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument,
                                          "program link failed: " + log);
    }

    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        resources_[index] = Resource{};
    } else {
        index = static_cast<HandleIndex>(resources_.size());
        resources_.push_back(Resource{});
        generations_.push_back(0);
    }
    auto& resource = resources_[index];
    resource.kind = Kind::Pipeline;
    resource.alive = true;
    resource.gl_id = program;
    resource.layout = desc.layout;
    resource.topology = desc.layout.topology;
    resource.cull_mode = desc.cull_mode;
    resource.depth_compare = desc.depth_compare;
    resource.depth_test = desc.depth_test;
    resource.depth_write = desc.depth_write;
    return Result<RenderHandle>::success(make_handle(index, generations_[index]));
}

Status GlRenderDevice::destroy(RenderHandle handle) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(handle)) {
        return Status::fail(ErrorCode::NotFound, "handle is not live");
    }
    auto& resource = resources_[handle.index];
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
    }
    resource.alive = false;
    ++generations_[handle.index];
    free_.push_back(handle.index);
    return Status::success();
}

Status GlRenderDevice::update_buffer(RenderHandle handle, std::span<const std::byte> data,
                                     std::uint64_t offset_bytes) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition, "device is not initialized");
    }
    if (!handle_live(handle)) {
        return Status::fail(ErrorCode::NotFound, "buffer handle is not live");
    }
    auto* resource = find(handle.index);
    if (resource->kind != Kind::Buffer) {
        return Status::fail(ErrorCode::InvalidArgument, "handle is not a buffer");
    }
    if (data.empty() || offset_bytes > resource->size_bytes ||
        data.size() > resource->size_bytes - offset_bytes) {
        return Status::fail(ErrorCode::OutOfRange, "buffer update is out of range");
    }
    // GL_ELEMENT_ARRAY_BUFFER bindings are VAO state; bind our VAO so the
    // upload lands on the same object draws will use.
    glBindVertexArray(vao_);
    glBindBuffer(static_cast<GLenum>(resource->target), resource->gl_id);
    glBufferSubData(static_cast<GLenum>(resource->target), static_cast<GLintptr>(offset_bytes),
                    static_cast<GLsizeiptr>(data.size()), data.data());
    glBindBuffer(static_cast<GLenum>(resource->target), 0);
    glBindVertexArray(0);
    return gl_error_status("glBufferSubData");
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

Status GlRenderDevice::clear(const ClearValue& value) {
    std::lock_guard<std::mutex> guard(mutex_);
    const Status status = require_frame();
    if (!status.ok()) {
        return status;
    }
    glClearColor(value.r, value.g, value.b, value.a);
    glClearDepth(static_cast<GLdouble>(value.depth));
    glClearStencil(static_cast<GLint>(value.stencil));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    return gl_error_status("glClear");
}

Status GlRenderDevice::draw(const DrawCall& call) {
    std::lock_guard<std::mutex> guard(mutex_);
    const Status status = require_frame();
    if (!status.ok()) {
        return status;
    }
    if (!valid(call)) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid draw call");
    }
    const auto* pipeline = find(call.pipeline.index);
    if (pipeline == nullptr || pipeline->kind != Kind::Pipeline || !handle_live(call.pipeline)) {
        return Status::fail(ErrorCode::NotFound, "pipeline handle is not live");
    }
    const auto* vertices = find(call.vertex_buffer.index);
    if (vertices == nullptr || vertices->kind != Kind::Buffer || !handle_live(call.vertex_buffer)) {
        return Status::fail(ErrorCode::NotFound, "vertex buffer handle is not live");
    }
    const auto* indices = find(call.index_buffer.index);
    if (call.index_count > 0 &&
        (indices == nullptr || indices->kind != Kind::Buffer || !handle_live(call.index_buffer))) {
        return Status::fail(ErrorCode::NotFound, "index buffer handle is not live");
    }

    glBindVertexArray(vao_);
    glUseProgram(pipeline->gl_id);

    if (pipeline->depth_test) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(compare_op(pipeline->depth_compare));
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glDepthMask(pipeline->depth_write ? GL_TRUE : GL_FALSE);
    const std::uint32_t cull = cull_face(pipeline->cull_mode);
    if (cull == 0) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
        glCullFace(static_cast<GLenum>(cull));
    }

    // Reset all attribute enables, then bind the pipeline's layout against the
    // draw's vertex buffer.
    for (std::uint32_t i = 0; i < kMaxVertexAttributes; ++i) {
        glDisableVertexAttribArray(i);
    }
    glBindBuffer(GL_ARRAY_BUFFER, vertices->gl_id);
    for (std::uint32_t i = 0; i < pipeline->layout.attribute_count; ++i) {
        const auto& attribute = pipeline->layout.attributes[i];
        const std::uint32_t stride =
            attribute.stride_bytes != 0 ? attribute.stride_bytes : attribute.components * 4;
        glVertexAttribPointer(
            static_cast<GLuint>(attribute.location), static_cast<GLint>(attribute.components),
            kAttributeType, GL_FALSE, static_cast<GLsizei>(stride),
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(attribute.offset_bytes)));
        glEnableVertexAttribArray(static_cast<GLuint>(attribute.location));
    }

    const std::uint32_t mode = primitive_mode(pipeline->topology);
    if (call.index_count > 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices->gl_id);
        glDrawElementsInstanced(mode, static_cast<GLsizei>(call.index_count), GL_UNSIGNED_INT,
                                nullptr, static_cast<GLsizei>(call.instance_count));
    } else {
        glDrawArraysInstanced(mode, 0, static_cast<GLsizei>(call.vertex_count),
                              static_cast<GLsizei>(call.instance_count));
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);
    return gl_error_status("draw");
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
