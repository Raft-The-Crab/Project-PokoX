#include "poko_test_harness.h"

#include <cstring>

#include "poko_asset_contracts.h"

POKO_TEST(asset_descriptors_validate) {
    POKO_CHECK(!poko::valid(poko::AssetDesc{}));
    poko::AssetDesc desc;
    desc.kind = poko::AssetKind::Mesh;
    desc.logical_path = "models/hero.mesh";
    POKO_CHECK(poko::valid(desc));
    desc.logical_path = "";
    POKO_CHECK(!poko::valid(desc));
}

POKO_TEST(kind_payloads_validate) {
    poko::MeshAssetDesc mesh;
    POKO_CHECK(!poko::valid(mesh));
    mesh.vertex_count = 3;
    mesh.submesh_count = 1;
    mesh.vertex_layout.attribute_count = 1;
    mesh.vertex_layout.attributes[0] = {0, 3, 12, 0};
    POKO_CHECK(poko::valid(mesh));

    poko::TextureAssetDesc tex;
    POKO_CHECK(!poko::valid(tex));
    tex.width = 4;
    tex.height = 4;
    tex.format = poko::PixelFormat::RGBA8;
    POKO_CHECK(poko::valid(tex));

    poko::AudioAssetDesc audio;
    POKO_CHECK(!poko::valid(audio));
    audio.sample_rate = 44100;
    audio.channels = 2;
    audio.bits_per_sample = 16;
    POKO_CHECK(poko::valid(audio));
}

POKO_TEST(state_names_are_stable) {
    POKO_CHECK(std::strcmp(poko::to_string(poko::AssetState::Ready), "ready") == 0);
    POKO_CHECK(std::strcmp(poko::to_string(poko::AssetState::Loading), "loading") == 0);
    POKO_CHECK(std::strcmp(poko::to_string(poko::AssetState::Failed), "failed") == 0);
}

POKO_TEST_MAIN()
