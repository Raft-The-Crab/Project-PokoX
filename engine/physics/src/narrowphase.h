#pragma once

#include "shape_ref.h"

namespace poko::physics::detail {

struct Pose {
    math::Vec3 position{};
    math::Quat rotation{};
};

// One contact: `normal` points from shape A toward shape B (the direction A
// must move to separate is -normal). `point` lies on the contact surface.
struct ContactPoint {
    math::Vec3 point{};
    math::Vec3 normal{0.0f, 1.0f, 0.0f};
    float depth = 0.0f;
};

inline constexpr int kMaxContacts = 8;

struct Manifold {
    ContactPoint points[kMaxContacts]{};
    int count = 0;

    void add(const ContactPoint& p) noexcept {
        if (count < kMaxContacts) {
            points[count++] = p;
        }
    }

    // Keeps the deepest contacts when full and merges near-duplicates, so
    // contacts gathered from many mesh triangles stay bounded and unique.
    void add_deepest(const ContactPoint& p) noexcept {
        for (int i = 0; i < count; ++i) {
            if (math::length_squared(points[i].point - p.point) < 1.0e-6f) {
                if (p.depth > points[i].depth) points[i] = p;
                return;
            }
        }
        if (count < kMaxContacts) {
            points[count++] = p;
            return;
        }
        int shallowest = 0;
        for (int i = 1; i < count; ++i) {
            if (points[i].depth < points[shallowest].depth) shallowest = i;
        }
        if (p.depth > points[shallowest].depth) points[shallowest] = p;
    }
};

// Exact narrow phase for every shape kind. Appends contacts to `out` and returns
// true when the shapes overlap. Mesh-vs-mesh is unsupported (meshes are static).
bool collide(const ShapeRef& a, const Pose& pa, const ShapeRef& b, const Pose& pb,
             Manifold& out) noexcept;

// Contacts of a shape against the half-space y <= height. Normal points down
// (from the shape toward the plane).
bool collide_ground(const ShapeRef& shape, const Pose& pose, float height, Manifold& out) noexcept;

// World-space half extents of an axis-aligned box about the body origin that
// contains the shape.
math::Vec3 aabb_half_extents(const ShapeRef& shape, const math::Quat& rotation) noexcept;

// Ray vs oriented shapes. `direction` must be normalized. On hit, writes the
// distance and world normal.
bool ray_shape(const math::Vec3& origin, const math::Vec3& direction, const ShapeRef& shape,
               const Pose& pose, float max_distance, float& out_t, math::Vec3& out_normal) noexcept;

// Triangle meshes: world-space triangles of `mesh` (posed by `pose`) whose bounds
// overlap the world box [lo, hi]; vertices are written 3 per triangle.
void mesh_triangles_in_box(const Geometry& mesh, const Pose& pose, const math::Vec3& lo,
                           const math::Vec3& hi, std::vector<math::Vec3>& world_vertices,
                           std::vector<std::uint32_t>& triangle_ids);

} // namespace poko::physics::detail
