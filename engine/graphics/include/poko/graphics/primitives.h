#pragma once

#include "poko/core/math.h"
#include "poko/graphics/mesh.h"

namespace poko::graphics {

// Procedural meshes with interleaved position (location 0) and normal
// (location 1), 24-byte stride, 32-bit indices, counter-clockwise outward
// winding. All are centred on the origin in object space.
MeshDesc make_cube_mesh(float half_extent = 0.5f);
// Flat square on the XZ plane facing +Y.
MeshDesc make_plane_mesh(float half_extent = 0.5f);
// Capsule along the Y axis; half_height = 0 gives a sphere.
MeshDesc make_capsule_mesh(float radius, float half_height, std::uint32_t segments = 24,
                           std::uint32_t rings = 8);
MeshDesc make_sphere_mesh(float radius, std::uint32_t segments = 24, std::uint32_t rings = 12);

// Row-major model matrix (same convention as math::Mat4) from translation,
// rotation and per-axis scale.
math::Mat4 make_model_matrix(const math::Vec3& position, const math::Quat& rotation,
                             const math::Vec3& scale = {1.0f, 1.0f, 1.0f}) noexcept;

} // namespace poko::graphics
