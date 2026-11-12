#pragma once

// Asset/resource contracts (systemplan/05). POD-only descriptors and opaque
// handles so the IAssetProvider seam stays free of backend dependencies.
//
// A "resource" here is a runtime asset (mesh data, texture payload, audio,
// font, scene blob) produced by the asset pipeline. The seam deals in
// descriptors + handles + async load requests; it does NOT own GPU objects
// (those come from IRenderDevice) or package formats (.pk, systemplan/13).

#include <array>
#include <cstdint>
#include <string_view>

#include "poko/core/render_contracts.h" // PixelFormat
#include "poko/core/types.h"

namespace poko {

using AssetHandle = TypedHandle;
using LoadRequestId = std::uint64_t;
inline constexpr LoadRequestId kInvalidLoadRequest = 0;

enum class AssetKind : std::uint8_t {
    Unknown = 0,
    Mesh,      // vertex/index buffers + submesh descriptors
    Texture,   // pixel payload -> IRenderDevice texture
    Material,  // shader + parameter blob (skeleton: opaque params)
    Shader,    // source or precompiled blob
    Audio,     // decoded PCM description
    Font,      // font atlas / glyph data
    Animation, // clip data
    Scene,     // serialized scene blob
    Skeleton,  // skinning hierarchy
    UISprite,  // 2D texture + metrics
    Raw,       // untyped blob passthrough
};

enum class AssetState : std::uint8_t {
    Unknown = 0,
    Unloaded,
    Loading,
    Ready,
    Failed,
    Evicted,
};

inline constexpr const char* to_string(AssetState state) noexcept {
    switch (state) {
    case AssetState::Unloaded:
        return "unloaded";
    case AssetState::Loading:
        return "loading";
    case AssetState::Ready:
        return "ready";
    case AssetState::Failed:
        return "failed";
    case AssetState::Evicted:
        return "evicted";
    case AssetState::Unknown:
        break;
    }
    return "unknown";
}

struct AssetDesc {
    AssetKind kind = AssetKind::Unknown;
    std::string_view logical_path; // e.g. "models/hero.mesh"; never a raw OS path
    std::string_view debug_name;
};

struct MeshAssetDesc {
    // Skeleton contract: payload stays opaque to the seam; the asset system
    // owns decoding. These fields describe the runtime shape only.
    std::uint32_t vertex_count = 0;
    std::uint32_t index_count = 0;
    std::uint32_t submesh_count = 0;
    IndexType index_type = IndexType::UInt32;
    VertexLayout vertex_layout{};
};

struct TextureAssetDesc {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    PixelFormat format = PixelFormat::Unknown;
    std::uint32_t mip_levels = 1;
};

struct AudioAssetDesc {
    std::uint32_t sample_rate = 0;
    std::uint16_t channels = 0;
    std::uint16_t bits_per_sample = 0;
    float duration_seconds = 0.0f;
};

// What a completed load yields. Exactly one member is meaningful per kind;
// backends fill the relevant one.
struct AssetPayload {
    MeshAssetDesc mesh{};
    TextureAssetDesc texture{};
    AudioAssetDesc audio{};
    std::uint64_t byte_size = 0; // raw blob size for Raw/other kinds
};

struct LoadResult {
    AssetHandle asset;
    AssetState state = AssetState::Failed;
    AssetPayload payload{};
};

// Input validation at the module boundary.
inline bool valid(const AssetDesc& desc) noexcept {
    return desc.kind != AssetKind::Unknown && !desc.logical_path.empty();
}

inline bool valid(const MeshAssetDesc& desc) noexcept {
    return desc.vertex_count > 0 && desc.submesh_count > 0 && valid(desc.vertex_layout);
}

inline bool valid(const TextureAssetDesc& desc) noexcept {
    return desc.width > 0 && desc.height > 0 && desc.format != PixelFormat::Unknown &&
           desc.mip_levels > 0;
}

inline bool valid(const AudioAssetDesc& desc) noexcept {
    return desc.sample_rate > 0 && desc.channels > 0 && desc.bits_per_sample > 0;
}

} // namespace poko
