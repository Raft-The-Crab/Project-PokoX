#pragma once

// Physics contracts (systemplan/06). POD-only descriptors and opaque handles
// so the IPhysicsWorld seam stays free of backend dependencies, same pattern
// as poko_render_contracts.h (DEC-003).
//
// Coordinate system (systemplan/06 requires this to be documented):
//   * Right-handed, +Y up, -Z forward — identical to poko_math.h.
//   * Distances in metres, mass in kilograms, time in seconds.
//   * Shapes are described in body-local space; BodyDesc::rotation orients
//     them in world space.
//   * collision_layer / collision_mask use bit flags; a pair collides when
//     (a.collision_layer & b.collision_mask) != 0 AND
//     (b.collision_layer & a.collision_mask) != 0.

#include <cstdint>
#include <string_view>

#include "poko/core/math.h"
#include "poko/core/types.h"

namespace poko {

using BodyHandle = TypedHandle;

enum class BodyType : std::uint8_t {
    Static,    // never moves; infinite effective mass
    Kinematic, // moves only via set_body_state; pushes dynamics
    Dynamic,   // simulated by step()
};

enum class ShapeKind : std::uint8_t {
    Sphere,
    Box,
    Capsule,
};

// Flat POD shape description; unused fields are ignored per kind.
//   Sphere:  radius
//   Box:     half_extents (full size = 2 * half_extents)
//   Capsule: radius + half_height (cylinder mid-section half height;
//            total half height = half_height + radius)
struct ShapeDesc {
    ShapeKind kind = ShapeKind::Sphere;
    float radius = 0.5f;
    math::Vec3 half_extents{0.5f, 0.5f, 0.5f};
    float half_height = 0.0f;
};

struct BodyDesc {
    BodyType type = BodyType::Dynamic;
    ShapeDesc shape;
    math::Vec3 position;
    math::Quat rotation; // identity by default (w = 1)
    math::Vec3 velocity;
    math::Vec3 angular_velocity;
    float mass = 1.0f;        // kg; ignored for Static
    float friction = 0.5f;    // 0..1
    float restitution = 0.0f; // 0..1
    std::uint32_t collision_layer = 1;
    std::uint32_t collision_mask = 0xFFFFFFFFu;
    // Triggers detect overlap (see CustomPhysicsWorld trigger events) but never
    // collide. They must be Static or Kinematic.
    bool is_trigger = false;
    std::string_view debug_name;
};

using JointHandle = TypedHandle;

enum class JointKind : std::uint8_t {
    Ball,     // anchors coincide, rotation free
    Distance, // anchors keep a fixed distance (rod)
    Hinge,    // anchors coincide, axes aligned (rotation about the axis only)
    Fixed,    // anchors coincide and relative rotation is locked
};

// Anchors and axes are in each body's local space. When body_b is an invalid
// handle the joint is anchored to the world and anchor_b / axis_b are world space.
struct JointDesc {
    JointKind kind = JointKind::Ball;
    BodyHandle body_a;
    BodyHandle body_b;
    math::Vec3 anchor_a;
    math::Vec3 anchor_b;
    math::Vec3 axis_a{0.0f, 1.0f, 0.0f}; // Hinge only
    math::Vec3 axis_b{0.0f, 1.0f, 0.0f}; // Hinge only
    float distance = -1.0f;              // Distance only; negative = current separation
};

struct BodyState {
    math::Vec3 position;
    math::Quat rotation;
    math::Vec3 velocity;
    math::Vec3 angular_velocity;
};

struct PhysicsWorldDesc {
    math::Vec3 gravity{0.0f, -9.81f, 0.0f};
    float fixed_delta_seconds = 1.0f / 60.0f;
};

struct Ray {
    math::Vec3 origin;
    math::Vec3 direction; // expected normalized; backends may normalize
};

struct RayHit {
    BodyHandle body;
    float distance = 0.0f; // metres along ray.direction
    math::Vec3 point;      // world-space hit position
    math::Vec3 normal;     // world-space surface normal at hit
};

// Input validation at the module boundary (same rule as render contracts).
inline bool valid(const ShapeDesc& shape) noexcept {
    switch (shape.kind) {
    case ShapeKind::Sphere:
        return shape.radius > 0.0f;
    case ShapeKind::Box:
        return shape.half_extents.x > 0.0f && shape.half_extents.y > 0.0f &&
               shape.half_extents.z > 0.0f;
    case ShapeKind::Capsule:
        return shape.radius > 0.0f && shape.half_height >= 0.0f;
    }
    return false;
}

inline bool valid(const BodyDesc& desc) noexcept {
    return valid(desc.shape) && (desc.type == BodyType::Static || desc.mass > 0.0f) &&
           desc.friction >= 0.0f && desc.friction <= 1.0f && desc.restitution >= 0.0f &&
           desc.restitution <= 1.0f && !(desc.is_trigger && desc.type == BodyType::Dynamic);
}

inline bool valid(const JointDesc& desc) noexcept {
    if (!is_valid(desc.body_a) || same_handle(desc.body_a, desc.body_b)) {
        return false;
    }
    if (desc.kind == JointKind::Hinge &&
        (math::length_squared(desc.axis_a) <= 0.0f || math::length_squared(desc.axis_b) <= 0.0f)) {
        return false;
    }
    return desc.kind != JointKind::Distance || desc.distance != 0.0f;
}

inline bool valid(const Ray& ray) noexcept {
    return math::length_squared(ray.direction) > 0.0f;
}

} // namespace poko
