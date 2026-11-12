#include "poko/renderer/gl_device.h"

#include <string>

#include "gl_conversions.h"

namespace poko {

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
    const std::uint32_t target = gl::buffer_target(desc.usage);
    glBindBuffer(target, id);
    glBufferData(target, static_cast<GLsizeiptr>(desc.size_bytes), nullptr,
                 gl::buffer_usage_hint(desc.usage));
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
    gl::GlFormat format{};
    if (!gl::pixel_format(desc.format, format)) {
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
        const std::uint32_t width = gl::mip_dimension(desc.width, level);
        const std::uint32_t height = gl::mip_dimension(desc.height, level);
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
    resource.width = desc.width;
    resource.height = desc.height;
    resource.mip_levels = desc.mip_levels;
    resource.pixel_format = static_cast<std::uint32_t>(desc.format);
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
    const GLuint id = glCreateShader(gl::shader_stage(desc.stage));
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

    // Bind each std140 uniform block to the binding point matching its block
    // index so DrawCall::uniform_buffer lands on the shader's blocks in order.
    GLint block_count = 0;
    glGetProgramInterfaceiv(program, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &block_count);
    for (GLint i = 0; i < block_count; ++i) {
        glUniformBlockBinding(program, static_cast<GLuint>(i), static_cast<GLuint>(i));
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
    resource.uniform_block_count = static_cast<std::uint32_t>(block_count);
    return Result<RenderHandle>::success(make_handle(index, generations_[index]));
}

Result<RenderHandle> GlRenderDevice::create_sampler(const SamplerDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition,
                                          "device is not initialized");
    }
    GLuint sampler = 0;
    glGenSamplers(1, &sampler);
    if (sampler == 0) {
        return Result<RenderHandle>::fail(ErrorCode::ResourceExhausted, "glGenSamplers failed");
    }
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER,
                        static_cast<GLint>(gl::filter_mode(desc.min_filter)));
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER,
                        static_cast<GLint>(gl::filter_mode(desc.mag_filter)));
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S,
                        static_cast<GLint>(gl::wrap_mode(desc.wrap_u)));
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T,
                        static_cast<GLint>(gl::wrap_mode(desc.wrap_v)));
    if (const Status status = gl_error_status("glSamplerParameteri"); !status.ok()) {
        glDeleteSamplers(1, &sampler);
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
    resource.kind = Kind::Sampler;
    resource.alive = true;
    resource.gl_id = sampler;
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
    case Kind::Sampler:
        glDeleteSamplers(1, &resource.gl_id);
        break;
    }
    resource.alive = false;
    ++generations_[handle.index];
    free_.push_back(handle.index);
    return Status::success();
}

} // namespace poko
