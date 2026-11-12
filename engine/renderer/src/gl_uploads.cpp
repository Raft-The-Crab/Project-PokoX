#include "poko/renderer/gl_device.h"

#include "gl_conversions.h"

namespace poko {

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

Status GlRenderDevice::update_texture(RenderHandle handle, std::uint32_t mip_level,
                                      std::span<const std::byte> data) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition, "device is not initialized");
    }
    if (!handle_live(handle)) {
        return Status::fail(ErrorCode::NotFound, "texture handle is not live");
    }
    auto* resource = find(handle.index);
    if (resource->kind != Kind::Texture) {
        return Status::fail(ErrorCode::InvalidArgument, "handle is not a texture");
    }
    if (mip_level >= resource->mip_levels) {
        return Status::fail(ErrorCode::OutOfRange, "mip level is out of range");
    }
    gl::GlFormat format{};
    if (!gl::pixel_format(static_cast<PixelFormat>(resource->pixel_format), format)) {
        return Status::fail(ErrorCode::InvalidArgument, "unsupported pixel format");
    }
    const std::uint64_t expected =
        static_cast<std::uint64_t>(gl::mip_dimension(resource->width, mip_level)) *
        gl::mip_dimension(resource->height, mip_level) * format.bytes_per_pixel;
    if (data.size() != expected) {
        return Status::fail(ErrorCode::OutOfRange, "texture data size does not match mip level");
    }
    glBindTexture(GL_TEXTURE_2D, resource->gl_id);
    glTexSubImage2D(GL_TEXTURE_2D, static_cast<GLint>(mip_level), 0, 0,
                    static_cast<GLsizei>(gl::mip_dimension(resource->width, mip_level)),
                    static_cast<GLsizei>(gl::mip_dimension(resource->height, mip_level)),
                    format.format, format.type, data.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return gl_error_status("glTexSubImage2D");
}

} // namespace poko
