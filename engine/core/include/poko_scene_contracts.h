#pragma once

// Scene/world contracts (systemplan/08). POD-only node descriptors and opaque
// handles so scene code stays backend-free. A "node" is a transform-bearing
// entity in the scene graph; components (mesh, light, script) attach later —
// the skeleton only owns hierarchy + transforms.
//
// Coordinates match poko_math.h: right-handed, +Y up, metres/radians.

#include <cstdint>
#include <string_view>

#include "poko_math.h"
#include "poko_types.h"

namespace poko {

using NodeHandle = TypedHandle;

// Local transform relative to the parent node. Scale is per-axis; uniform
// scale 1 keeps nodes unscaled.
struct Transform {
    math::Vec3 position;
    math::Quat rotation; // identity by default
    math::Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct NodeDesc {
    std::string_view name;
    NodeHandle parent; // invalid = scene root
    Transform local{}; // local transform relative to parent
};

inline Transform identity_transform() noexcept {
    return Transform{};
}

inline bool valid(const Transform& transform) noexcept {
    // Reject NaN-ish scale collapse; position/rotation are unconstrained.
    return transform.scale.x != 0.0f && transform.scale.y != 0.0f && transform.scale.z != 0.0f;
}

inline bool valid(const NodeDesc& desc) noexcept {
    return valid(desc.local);
}

} // namespace poko
