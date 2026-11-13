#pragma once

#include "poko/core/physics_contracts.h"

namespace poko::physics::detail {

// A convex shape as "core + margin": the core is a point, segment or oriented
// box and the shape is every point within `margin` of it. Sphere = point core,
// capsule = segment core, box = box core with zero margin. GJK works on cores,
// so distances between rounded shapes are exact and cheap.
struct ConvexCore {
    enum class Kind : std::uint8_t { Point, Segment, Box } kind = Kind::Point;
    math::Vec3 center{};
    math::Vec3 p0{};
    math::Vec3 p1{};
    math::Vec3 axes[3]{};
    math::Vec3 half{};
    float margin = 0.0f;
};

ConvexCore make_core(const ShapeDesc& shape, const math::Vec3& position,
                     const math::Quat& rotation) noexcept;
math::Vec3 support(const ConvexCore& core, const math::Vec3& direction) noexcept;

// Farthest distance from the body origin to any point of the shape.
float bounding_radius(const ShapeDesc& shape) noexcept;

struct CoreDistance {
    float distance = 0.0f; // between cores; 0 when they intersect
    bool intersecting = false;
    math::Vec3 point_a{};
    math::Vec3 point_b{};
};

CoreDistance core_distance(const ConvexCore& a, const ConvexCore& b) noexcept;

// Rigid motion over one step: position and orientation at t = 0 plus constant
// linear and angular velocity.
struct Motion {
    math::Vec3 position{};
    math::Quat rotation{};
    math::Vec3 velocity{};
    math::Vec3 spin{};
};

math::Quat advance_rotation(const math::Quat& rotation, const math::Vec3& spin, float t) noexcept;

struct ImpactResult {
    float time = 0.0f;    // seconds into the step when the surfaces first touch
    float closing = 0.0f; // approach speed along the contact normal at that time
    math::Vec3 normal{};  // from A toward B
};

// Conservative advancement: the earliest time in [0, duration] at which shape A
// (moving) comes within `tolerance` of shape B, accounting for rotation of both.
// Returns false for a miss, for shapes already overlapping or touching and
// separating at t = 0 (the contact solver owns those).
bool time_of_impact(const ShapeDesc& shape_a, const Motion& a, const ShapeDesc& shape_b,
                    const Motion& b, float duration, float tolerance, ImpactResult& out) noexcept;

} // namespace poko::physics::detail
