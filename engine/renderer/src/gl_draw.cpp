#include "poko/renderer/gl_device.h"

#include "gl_conversions.h"

namespace poko {

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
    const auto* uniforms =
        is_valid(call.uniform_buffer) ? find(call.uniform_buffer.index) : nullptr;
    if (is_valid(call.uniform_buffer) && (uniforms == nullptr || uniforms->kind != Kind::Buffer ||
                                          !handle_live(call.uniform_buffer))) {
        return Status::fail(ErrorCode::NotFound, "uniform buffer handle is not live");
    }
    for (std::uint32_t i = 0; i < call.texture_count; ++i) {
        const auto* texture = is_valid(call.textures[i]) ? find(call.textures[i].index) : nullptr;
        const auto* sampler = is_valid(call.samplers[i]) ? find(call.samplers[i].index) : nullptr;
        if (texture == nullptr || texture->kind != Kind::Texture ||
            !handle_live(call.textures[i]) || sampler == nullptr ||
            sampler->kind != Kind::Sampler || !handle_live(call.samplers[i])) {
            return Status::fail(ErrorCode::NotFound, "texture/sampler handle is not live");
        }
    }

    glBindVertexArray(vao_);
    glUseProgram(pipeline->gl_id);

    if (pipeline->depth_test) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(gl::compare_op(pipeline->depth_compare));
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glDepthMask(pipeline->depth_write ? GL_TRUE : GL_FALSE);
    const std::uint32_t cull = gl::cull_face(pipeline->cull_mode);
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
            gl::kAttributeType, GL_FALSE, static_cast<GLsizei>(stride),
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(attribute.offset_bytes)));
        glEnableVertexAttribArray(static_cast<GLuint>(attribute.location));
    }

    if (uniforms != nullptr) {
        const std::uint64_t range = uniforms->size_bytes > call.uniform_offset
                                        ? uniforms->size_bytes - call.uniform_offset
                                        : 0;
        for (std::uint32_t i = 0; i < pipeline->uniform_block_count; ++i) {
            glBindBufferRange(GL_UNIFORM_BUFFER, i, uniforms->gl_id,
                              static_cast<GLintptr>(call.uniform_offset),
                              static_cast<GLsizeiptr>(range));
        }
    }

    for (std::uint32_t i = 0; i < call.texture_count; ++i) {
        const auto* texture = find(call.textures[i].index);
        const auto* sampler = find(call.samplers[i].index);
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, texture->gl_id);
        glBindSampler(i, sampler->gl_id);
    }

    const std::uint32_t mode = gl::primitive_mode(pipeline->topology);
    const GLenum index_type =
        call.index_type == IndexType::UInt16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    if (call.index_count > 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices->gl_id);
        glDrawElementsInstanced(mode, static_cast<GLsizei>(call.index_count), index_type, nullptr,
                                static_cast<GLsizei>(call.instance_count));
    } else {
        glDrawArraysInstanced(mode, 0, static_cast<GLsizei>(call.vertex_count),
                              static_cast<GLsizei>(call.instance_count));
    }

    for (std::uint32_t i = 0; i < call.texture_count; ++i) {
        glBindSampler(i, 0);
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);
    return gl_error_status("draw");
}

} // namespace poko
