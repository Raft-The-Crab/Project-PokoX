#include "poko/graphics/draw_call.h"

#include <cstdint>

namespace poko::graphics {
namespace {

bool same_layout(const VertexLayout& a, const VertexLayout& b) noexcept {
    if (a.attribute_count != b.attribute_count || a.topology != b.topology) {
        return false;
    }
    for (std::uint32_t i = 0; i < a.attribute_count; ++i) {
        const auto& x = a.attributes[i];
        const auto& y = b.attributes[i];
        if (x.location != y.location || x.components != y.components ||
            x.stride_bytes != y.stride_bytes || x.offset_bytes != y.offset_bytes) {
            return false;
        }
    }
    return true;
}

} // namespace

Result<DrawCall> DrawCallBuilder::build(MeshHandle mesh, MaterialHandle material,
                                        const DrawOptions& options) const {
    auto mesh_result = meshes_.get(mesh);
    if (!mesh_result.ok()) {
        return Result<DrawCall>::fail(mesh_result.error().code, mesh_result.error().message);
    }
    auto material_result = materials_.get(material);
    if (!material_result.ok()) {
        return Result<DrawCall>::fail(material_result.error().code,
                                      material_result.error().message);
    }
    const GpuMesh& gm = mesh_result.value();
    const GpuMaterial& mat = material_result.value();

    if (!same_layout(gm.layout, mat.layout)) {
        return Result<DrawCall>::fail(ErrorCode::InvalidArgument,
                                      "mesh and material vertex layouts differ");
    }
    for (std::uint32_t i = 0; i < mat.texture_count; ++i) {
        if (!is_valid(mat.textures[i]) || !is_valid(mat.samplers[i])) {
            return Result<DrawCall>::fail(ErrorCode::FailedPrecondition,
                                          "material texture binding is incomplete");
        }
    }

    DrawCall call;
    call.pipeline = mat.pipeline;
    call.vertex_buffer = gm.vertex_buffer;
    call.index_buffer = gm.index_buffer;
    call.uniform_buffer = mat.uniform_buffer;
    call.uniform_offset = is_valid(mat.uniform_buffer) ? options.uniform_offset : 0;
    call.textures = mat.textures;
    call.samplers = mat.samplers;
    call.texture_count = mat.texture_count;
    call.index_type = gm.index_type;
    call.vertex_count = gm.vertex_count;
    call.index_count = gm.index_count;
    call.instance_count = options.instance_count;
    call.sort_key = options.sort_key;

    if (!::poko::valid(call)) {
        return Result<DrawCall>::fail(ErrorCode::InvalidArgument, "assembled draw call is invalid");
    }
    return Result<DrawCall>::success(call);
}

} // namespace poko::graphics
