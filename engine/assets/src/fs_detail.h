#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "poko/core/asset_contracts.h"

namespace poko::assets::detail {

// Minimal, versioned PokoX asset container. A file is a 20-byte little-endian
// header followed by an opaque payload region. The header is deliberately
// small: real importers (glTF, PNG, WAV, ...) replace this decoder later
// without changing the IAssetProvider seam.
//
//   magic "PKX1" | kind | a | b | c | payload...
//
// kind must equal AssetDesc::kind. Fields by kind:
//   Mesh    a=vertex_count b=index_count c=submesh_count (vec3 layout assumed)
//   Texture a=width        b=height       c=PixelFormat (mip_levels = 1)
//   Audio   a=sample_rate  b=channels     c=bits_per_sample
//   others  a/b/c unused, payload is opaque
struct PkxHeader {
    std::array<char, 4> magic{'P', 'K', 'X', '1'};
    std::uint32_t kind = 0;
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    std::uint32_t c = 0;
};

static_assert(sizeof(PkxHeader) == 20, "PKX1 header must be 20 bytes");

// Decodes a whole file into a payload. Returns false for a short file, a bad
// magic, a kind mismatch or invalid structured fields.
inline bool decode(std::span<const std::byte> bytes, AssetKind kind,
                   AssetPayload& out) noexcept {
    if (bytes.size() < sizeof(PkxHeader)) {
        return false;
    }
    PkxHeader header;
    std::memcpy(&header, bytes.data(), sizeof(header));
    if (header.magic != std::array<char, 4>{'P', 'K', 'X', '1'} ||
        header.kind != static_cast<std::uint32_t>(kind)) {
        return false;
    }

    out = AssetPayload{};
    out.byte_size = bytes.size() - sizeof(PkxHeader);
    switch (kind) {
    case AssetKind::Mesh:
        out.mesh.vertex_count = header.a;
        out.mesh.index_count = header.b;
        out.mesh.submesh_count = header.c;
        out.mesh.index_type = IndexType::UInt32;
        out.mesh.vertex_layout.attribute_count = 1;
        out.mesh.vertex_layout.attributes[0] = {0, 3, 12, 0};
        return ::poko::valid(out.mesh);
    case AssetKind::Texture:
        if (header.c > static_cast<std::uint32_t>(PixelFormat::Depth32F)) {
            return false;
        }
        out.texture.width = header.a;
        out.texture.height = header.b;
        out.texture.format = static_cast<PixelFormat>(header.c);
        out.texture.mip_levels = 1;
        return ::poko::valid(out.texture);
    case AssetKind::Audio:
        if (header.b == 0 || header.b > 0xFFFFu || header.c == 0 || header.c > 0xFFFFu) {
            return false;
        }
        out.audio.sample_rate = header.a;
        out.audio.channels = static_cast<std::uint16_t>(header.b);
        out.audio.bits_per_sample = static_cast<std::uint16_t>(header.c);
        if (!::poko::valid(out.audio)) {
            return false;
        }
        {
            const std::uint64_t bytes_per_frame =
                static_cast<std::uint64_t>(header.b) * (header.c / 8u);
            const std::uint64_t frames = bytes_per_frame > 0 ? out.byte_size / bytes_per_frame : 0;
            out.audio.duration_seconds =
                header.a > 0 ? static_cast<float>(static_cast<double>(frames) / header.a) : 0.0f;
        }
        return true;
    case AssetKind::Unknown:
        return false;
    default:
        return true; // opaque payload
    }
}

} // namespace poko::assets::detail
