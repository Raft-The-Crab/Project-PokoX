#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "poko/core/math.h"
#include "poko/core/result.h"

namespace poko::physics::detail {

inline constexpr std::size_t kMaxHullPoints = 256;
inline constexpr std::size_t kMaxMeshTriangles = 100000;

// One planar polygon of a convex hull. `verts` index Geometry::vertices and run
// counter-clockwise seen from outside; the plane is dot(normal, p) = offset.
struct HullFace {
    math::Vec3 normal{};
    float offset = 0.0f;
    std::vector<std::uint16_t> verts;
};

// An undirected hull edge with the two faces that meet at it.
struct HullEdge {
    std::uint16_t a = 0;
    std::uint16_t b = 0;
    std::uint16_t face_a = 0;
    std::uint16_t face_b = 0;
};

struct BvhNode {
    math::Vec3 lo{};
    math::Vec3 hi{};
    std::int32_t left = -1;  // child indices, or -1 for a leaf
    std::int32_t right = -1;
    std::uint32_t first = 0; // leaf: range in Geometry::tri_order
    std::uint32_t count = 0;
};

// Immutable collision geometry shared by any number of bodies.
struct Geometry {
    enum class Kind : std::uint8_t { Hull, Mesh } kind = Kind::Hull;

    std::vector<math::Vec3> vertices;
    // Hull only.
    std::vector<HullFace> faces;
    std::vector<HullEdge> edges;
    // Mesh only: triangle index triples, a BVH over them, and the BVH leaf order.
    std::vector<std::uint32_t> indices;
    std::vector<BvhNode> nodes;
    std::vector<std::uint32_t> tri_order;

    math::Vec3 half{};  // max |coordinate| per axis: a box about the origin that contains it
    float radius = 0.0f; // farthest vertex from the origin
    float inner = 0.0f;  // hull: distance from the origin to the nearest face
    math::Vec3 inertia_per_mass{}; // diagonal of the inertia tensor about the origin / mass
    int users = 0;       // bodies referencing this geometry
};

// Builds the convex hull of a point cloud (incremental, coplanar triangles merged
// into polygons). Fails on non-finite input, more than kMaxHullPoints points, or a
// degenerate (collinear or planar) cloud.
Result<std::shared_ptr<Geometry>> build_hull(std::span<const math::Vec3> points);

// Builds a triangle mesh with a BVH. Degenerate triangles are dropped; fails on
// bad indices, non-finite vertices or more than kMaxMeshTriangles triangles.
Result<std::shared_ptr<Geometry>> build_mesh(std::span<const math::Vec3> vertices,
                                             std::span<const std::uint32_t> indices);

// Appends the indices of triangles whose bounds overlap [lo, hi] (mesh local space).
void mesh_query(const Geometry& mesh, const math::Vec3& lo, const math::Vec3& hi,
                std::vector<std::uint32_t>& out);

// Mesh triangle vertices in mesh local space.
inline void mesh_triangle(const Geometry& mesh, std::uint32_t triangle, math::Vec3 out[3]) {
    for (int i = 0; i < 3; ++i) out[i] = mesh.vertices[mesh.indices[triangle * 3 + static_cast<std::uint32_t>(i)]];
}

} // namespace poko::physics::detail
