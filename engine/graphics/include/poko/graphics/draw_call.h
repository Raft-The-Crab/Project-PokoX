#pragma once

#include <cstdint>

#include "poko/core/render_contracts.h"
#include "poko/core/result.h"
#include "poko/core/types.h"
#include "poko/graphics/material.h"
#include "poko/graphics/mesh.h"

namespace poko::graphics {

// Per-instance overrides applied when assembling a DrawCall.
struct DrawOptions {
    std::uint32_t instance_count = 1;
    std::uint32_t sort_key = 0;
    std::uint64_t uniform_offset = 0;
};

// Assembles a fully-formed, validated DrawCall from a mesh and a material.
// Resolves both libraries by handle, checks mesh/material layout compatibility
// and texture binding validity, then returns a call ready for
// IRenderDevice::draw(). Owns no GPU resources.
// Thread-safety contract: holds const references; callers own the libraries.
class DrawCallBuilder {
public:
    DrawCallBuilder(const MeshLibrary& meshes, const MaterialLibrary& materials) noexcept
        : meshes_(meshes), materials_(materials) {}

    Result<DrawCall> build(MeshHandle mesh, MaterialHandle material,
                           const DrawOptions& options = {}) const;

private:
    const MeshLibrary& meshes_;
    const MaterialLibrary& materials_;
};

} // namespace poko::graphics
