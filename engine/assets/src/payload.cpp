#include "poko/assets/provider.h"

namespace poko {

AssetPayload NullAssetProvider::synthesize_payload(AssetKind kind) noexcept {
    AssetPayload payload;
    switch (kind) {
    case AssetKind::Mesh:
        payload.mesh.vertex_count = 3;
        payload.mesh.index_count = 3;
        payload.mesh.submesh_count = 1;
        payload.mesh.index_type = IndexType::UInt32;
        payload.mesh.vertex_layout.attribute_count = 1;
        payload.mesh.vertex_layout.attributes[0] = {0, 3, 12, 0};
        payload.byte_size = 36;
        break;
    case AssetKind::Texture:
        payload.texture.width = 4;
        payload.texture.height = 4;
        payload.texture.format = PixelFormat::RGBA8;
        payload.texture.mip_levels = 1;
        payload.byte_size = 64;
        break;
    case AssetKind::Audio:
        payload.audio.sample_rate = 44100;
        payload.audio.channels = 2;
        payload.audio.bits_per_sample = 16;
        payload.audio.duration_seconds = 1.0f;
        payload.byte_size = 176400;
        break;
    case AssetKind::Material:
    case AssetKind::Shader:
    case AssetKind::Font:
    case AssetKind::Animation:
    case AssetKind::Scene:
    case AssetKind::Skeleton:
    case AssetKind::UISprite:
    case AssetKind::Raw:
        payload.byte_size = 256;
        break;
    case AssetKind::Unknown:
        break;
    }
    return payload;
}

} // namespace poko
