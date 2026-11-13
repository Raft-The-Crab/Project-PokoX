#include "poko/graphics/scene_renderer.h"

#include <cmath>
#include <cstring>

namespace poko::graphics {
namespace {

constexpr std::uint32_t kSlotBytes = 256;

constexpr const char* kVertexSource = R"(#version 450
// poko:lit_mesh
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(std140, binding = 0) uniform Draw {
    layout(row_major) mat4 mvp;
    vec4 color;
    vec4 light;
};
layout(location = 0) out vec3 v_normal;
void main() {
    v_normal = a_normal;
    gl_Position = mvp * vec4(a_position, 1.0);
}
)";

constexpr const char* kFragmentSource = R"(#version 450
// poko:lambert
layout(location = 0) in vec3 v_normal;
layout(std140, binding = 0) uniform Draw {
    layout(row_major) mat4 mvp;
    vec4 color;
    vec4 light;
};
layout(location = 0) out vec4 out_color;
void main() {
    float diffuse = max(dot(normalize(v_normal), normalize(light.xyz)), 0.0);
    out_color = vec4(color.rgb * (light.w + (1.0 - light.w) * diffuse), color.a);
}
)";

// Inverse of the upper 3x3 (cofactor form); falls back to identity if singular.
math::Vec3 inverse_rotate_scale(const math::Mat4& m, const math::Vec3& v) noexcept {
    const math::Vec3 c0{m.at(0, 0), m.at(1, 0), m.at(2, 0)};
    const math::Vec3 c1{m.at(0, 1), m.at(1, 1), m.at(2, 1)};
    const math::Vec3 c2{m.at(0, 2), m.at(1, 2), m.at(2, 2)};
    const float det = math::dot(c0, math::cross(c1, c2));
    if (std::abs(det) < 1.0e-12f) {
        return v;
    }
    // Rows of the inverse are cross products of the other two columns.
    const math::Vec3 r0 = math::cross(c1, c2) / det;
    const math::Vec3 r1 = math::cross(c2, c0) / det;
    const math::Vec3 r2 = math::cross(c0, c1) / det;
    return {math::dot(r0, v), math::dot(r1, v), math::dot(r2, v)};
}

} // namespace

Status SceneRenderer::initialize() {
    if (initialized_) {
        return Status::success();
    }
    ShaderDesc vs;
    vs.stage = ShaderStage::Vertex;
    vs.source = kVertexSource;
    ShaderDesc fs;
    fs.stage = ShaderStage::Fragment;
    fs.source = kFragmentSource;
    const auto vertex = device_.create_shader(vs);
    if (!vertex.ok()) return Status::fail(vertex.error().code, vertex.error().message);
    const auto fragment = device_.create_shader(fs);
    if (!fragment.ok()) return Status::fail(fragment.error().code, fragment.error().message);

    PipelineDesc pipeline;
    pipeline.vertex_shader = vertex.value();
    pipeline.fragment_shader = fragment.value();
    pipeline.layout.attribute_count = 2;
    pipeline.layout.attributes[0] = {0, 3, 24, 0};
    pipeline.layout.attributes[1] = {1, 3, 24, 12};
    const auto created = device_.create_pipeline(pipeline);
    if (!created.ok()) return Status::fail(created.error().code, created.error().message);
    pipeline_ = created.value();

    BufferDesc ring;
    ring.size_bytes = static_cast<std::uint64_t>(max_draws_) * kSlotBytes;
    ring.usage = BufferUsage::Uniform;
    ring.host_visible = true;
    const auto buffer = device_.create_buffer(ring);
    if (!buffer.ok()) return Status::fail(buffer.error().code, buffer.error().message);
    uniform_ring_ = buffer.value();
    initialized_ = true;
    return Status::success();
}

Result<std::uint32_t> SceneRenderer::add_mesh(const MeshDesc& desc) {
    const auto handle = meshes_.create(desc);
    if (!handle.ok()) {
        return Result<std::uint32_t>::fail(handle.error().code, handle.error().message);
    }
    mesh_handles_.push_back(handle.value());
    return Result<std::uint32_t>::success(static_cast<std::uint32_t>(mesh_handles_.size() - 1));
}

void SceneRenderer::begin(const math::Mat4& view_projection, const math::Vec3& light_direction,
                          float ambient) noexcept {
    view_projection_ = view_projection;
    light_ = light_direction;
    ambient_ = math::clamp(ambient, 0.0f, 1.0f);
    draw_index_ = 0;
}

Status SceneRenderer::draw(std::uint32_t mesh, const math::Mat4& model, const math::Vec3& color) {
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition, "scene renderer is not initialized");
    }
    if (mesh >= mesh_handles_.size()) {
        return Status::fail(ErrorCode::NotFound, "unknown mesh index");
    }
    if (draw_index_ >= max_draws_) {
        return Status::fail(ErrorCode::ResourceExhausted, "per-frame draw budget exhausted");
    }
    const auto gpu = meshes_.get(mesh_handles_[mesh]);
    if (!gpu.ok()) return Status::fail(gpu.error().code, gpu.error().message);

    DrawUniforms uniforms{};
    const math::Mat4 mvp = math::multiply(view_projection_, model);
    std::memcpy(uniforms.mvp, &mvp, sizeof(uniforms.mvp));
    uniforms.color[0] = color.x;
    uniforms.color[1] = color.y;
    uniforms.color[2] = color.z;
    uniforms.color[3] = 1.0f;
    const math::Vec3 light = inverse_rotate_scale(model, light_);
    uniforms.light[0] = light.x;
    uniforms.light[1] = light.y;
    uniforms.light[2] = light.z;
    uniforms.light[3] = ambient_;

    const std::uint64_t offset = static_cast<std::uint64_t>(draw_index_) * kSlotBytes;
    const auto status = device_.update_buffer(
        uniform_ring_, {reinterpret_cast<const std::byte*>(&uniforms), sizeof(uniforms)}, offset);
    if (!status.ok()) return status;

    DrawCall call;
    call.pipeline = pipeline_;
    call.vertex_buffer = gpu.value().vertex_buffer;
    call.index_buffer = gpu.value().index_buffer;
    call.index_type = gpu.value().index_type;
    call.vertex_count = gpu.value().vertex_count;
    call.index_count = gpu.value().index_count;
    call.uniform_buffer = uniform_ring_;
    call.uniform_offset = offset;
    const auto drawn = device_.draw(call);
    if (drawn.ok()) ++draw_index_;
    return drawn;
}

} // namespace poko::graphics
