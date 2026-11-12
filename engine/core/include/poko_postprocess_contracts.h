#pragma once

// Post-process contracts (systemplan/04): vendor-neutral descriptions of
// screen-space effects applied after the scene pass. Backends map these to
// shaders/passes; the skeleton only defines parameters + ordering.

#include <cstdint>

#include "poko_math.h"
#include "poko_types.h"

namespace poko {

using PostProcessHandle = TypedHandle;

enum class ToneMap : std::uint8_t {
    Linear = 0, // no mapping (debug)
    Reinhard,
    ACES,    // Academy Color Encoding System approx
    Neutral, // neutral filmic
};

enum class PostProcessKind : std::uint8_t {
    Unknown = 0,
    ToneMap,    // exposure + operator
    Bloom,      // bright-pass + blur + composite
    ColorGrade, // LUT/shadow-mid-highlight adjustments
    Vignette,   // radial darkening
    FXAA,       // fast approximate AA
    SSAO,       // screen-space ambient occlusion (placeholder params)
};

struct ToneMapDesc {
    ToneMap operator_ = ToneMap::ACES;
    float exposure = 1.0f; // linear multiplier before mapping
};

struct BloomDesc {
    float threshold = 1.0f; // luminance cutoff for bright pass
    float intensity = 0.5f; // bloom contribution weight
    float radius = 4.0f;    // blur radius in pixels (approx)
};

struct ColorGradeDesc {
    math::Vec3 shadows{1.0f, 1.0f, 1.0f}; // RGB multiply in shadows
    math::Vec3 midtones{1.0f, 1.0f, 1.0f};
    math::Vec3 highlights{1.0f, 1.0f, 1.0f};
    float saturation = 1.0f; // 0 = grayscale, 1 = identity, >1 = punchy
};

struct VignetteDesc {
    float intensity = 0.5f;  // 0 = off, 1 = fully darkened edges
    float smoothness = 0.5f; // falloff softness 0..1
    float radius = 0.75f;    // inner clear radius 0..1 (uv space)
};

// Type-tagged union by value.
struct PostProcessDesc {
    PostProcessKind kind = PostProcessKind::Unknown;
    ToneMapDesc tone_map{};
    BloomDesc bloom{};
    ColorGradeDesc color_grade{};
    VignetteDesc vignette{};
};

// An ordered chain entry: which effect + its parameters.
struct PostProcessChain {
    PostProcessKind order[8] = {};
    std::uint32_t count = 0;
};

inline bool valid(const ToneMapDesc& desc) noexcept {
    return desc.exposure >= 0.0f;
}

inline bool valid(const BloomDesc& desc) noexcept {
    return desc.threshold >= 0.0f && desc.intensity >= 0.0f && desc.radius >= 0.0f;
}

inline bool valid(const ColorGradeDesc& desc) noexcept {
    return desc.saturation >= 0.0f;
}

inline bool valid(const VignetteDesc& desc) noexcept {
    return desc.intensity >= 0.0f && desc.intensity <= 1.0f && desc.smoothness >= 0.0f &&
           desc.smoothness <= 1.0f && desc.radius >= 0.0f && desc.radius <= 1.0f;
}

inline bool valid(const PostProcessDesc& desc) noexcept {
    switch (desc.kind) {
    case PostProcessKind::ToneMap:
        return valid(desc.tone_map);
    case PostProcessKind::Bloom:
        return valid(desc.bloom);
    case PostProcessKind::ColorGrade:
        return valid(desc.color_grade);
    case PostProcessKind::Vignette:
        return valid(desc.vignette);
    case PostProcessKind::FXAA:
    case PostProcessKind::SSAO:
        return true; // parameterless for now
    case PostProcessKind::Unknown:
        break;
    }
    return false;
}

} // namespace poko
