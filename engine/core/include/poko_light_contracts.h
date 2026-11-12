#pragma once

// Lighting contracts (systemplan/04): vendor-neutral light descriptions for
// the renderer's forward/deferred passes. POD-only; backends map these to
// their shading model. No shadows yet — shadow maps are a later milestone.

#include <cstdint>

#include "poko_math.h"
#include "poko_types.h"

namespace poko {

using LightHandle = TypedHandle;

enum class LightKind : std::uint8_t {
    Unknown = 0,
    Directional, // sun/moon: direction + color, no position
    Point,       // omni: position + radius falloff
    Spot,        // cone: position + direction + inner/outer angles
};

// Linear RGB color, components typically 0..1 (HDR values allowed).
struct LightColor {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
};

struct DirectionalLightDesc {
    math::Vec3 direction; // unit vector TOWARD the light (e.g. sun)
    LightColor color;
    float intensity = 1.0f;
};

struct PointLightDesc {
    math::Vec3 position;
    LightColor color;
    float intensity = 1.0f;
    float radius = 10.0f; // metres; 0 = infinite (rare)
};

struct SpotLightDesc {
    math::Vec3 position;
    math::Vec3 direction; // unit vector the cone points along
    LightColor color;
    float intensity = 1.0f;
    float radius = 10.0f;
    float inner_angle = 0.5f; // radians, full intensity inside
    float outer_angle = 0.7f; // radians, zero beyond
};

// Type-tagged union by value (small enough to stay POD-friendly).
struct LightDesc {
    LightKind kind = LightKind::Unknown;
    DirectionalLightDesc directional{};
    PointLightDesc point{};
    SpotLightDesc spot{};
};

// Ambient/hemisphere fill — keeps unlit directions from going pure black.
struct AmbientLight {
    LightColor sky{0.2f, 0.2f, 0.25f};
    LightColor ground{0.1f, 0.1f, 0.1f};
    float intensity = 1.0f;
};

inline bool valid(const DirectionalLightDesc& desc) noexcept {
    return math::length_squared(desc.direction) > 0.0f && desc.intensity >= 0.0f;
}

inline bool valid(const PointLightDesc& desc) noexcept {
    return desc.radius >= 0.0f && desc.intensity >= 0.0f;
}

inline bool valid(const SpotLightDesc& desc) noexcept {
    return desc.radius >= 0.0f && desc.intensity >= 0.0f && desc.inner_angle >= 0.0f &&
           desc.outer_angle > desc.inner_angle;
}

inline bool valid(const LightDesc& desc) noexcept {
    switch (desc.kind) {
    case LightKind::Directional:
        return valid(desc.directional);
    case LightKind::Point:
        return valid(desc.point);
    case LightKind::Spot:
        return valid(desc.spot);
    case LightKind::Unknown:
        break;
    }
    return false;
}

} // namespace poko
