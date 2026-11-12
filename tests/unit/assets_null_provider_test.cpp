#include "poko_test_harness.h"

#include <cstring>

#include "poko_asset_provider.h"

POKO_TEST(provider_requires_initialize) {
    poko::NullAssetProvider provider;
    poko::AssetDesc desc;
    desc.kind = poko::AssetKind::Raw;
    desc.logical_path = "data/blob.bin";
    POKO_CHECK(!provider.request_load(desc).ok());
    POKO_CHECK(provider.initialize());
    POKO_CHECK(provider.request_load(desc).ok());
}

POKO_TEST(request_rejects_invalid_desc) {
    poko::NullAssetProvider provider;
    POKO_CHECK(provider.initialize());
    POKO_CHECK(!provider.request_load(poko::AssetDesc{}).ok());
}

POKO_TEST(load_completes_on_update) {
    poko::NullAssetProvider provider;
    POKO_CHECK(provider.initialize());
    poko::AssetDesc desc;
    desc.kind = poko::AssetKind::Mesh;
    desc.logical_path = "models/tri.mesh";
    const auto request = provider.request_load(desc);
    POKO_CHECK(request.ok());

    poko::LoadResult out{};
    POKO_CHECK(!provider.poll_load(request.value(), out)); // pending

    provider.update();
    POKO_CHECK(provider.poll_load(request.value(), out));
    POKO_CHECK(out.state == poko::AssetState::Ready);
    POKO_CHECK(poko::is_valid(out.asset));
    POKO_CHECK(provider.asset_state(out.asset) == poko::AssetState::Ready);
    POKO_CHECK_EQ(out.payload.mesh.vertex_count, 3u);

    const auto asset = provider.get_asset(out.asset);
    POKO_CHECK(asset.ok());
    POKO_CHECK_EQ(asset.value().payload.byte_size, 36u);
}

POKO_TEST(cancel_and_release) {
    poko::NullAssetProvider provider;
    POKO_CHECK(provider.initialize());
    poko::AssetDesc desc;
    desc.kind = poko::AssetKind::Texture;
    desc.logical_path = "tex/checker.png";
    const auto request = provider.request_load(desc);
    POKO_CHECK(request.ok());

    POKO_CHECK(provider.cancel_load(request.value()).ok());
    poko::LoadResult out{};
    provider.update();
    POKO_CHECK(!provider.poll_load(request.value(), out)); // cancelled

    const auto request2 = provider.request_load(desc);
    POKO_CHECK(request2.ok());
    provider.update();
    poko::LoadResult out2{};
    POKO_CHECK(provider.poll_load(request2.value(), out2));
    POKO_CHECK(provider.release_asset(out2.asset).ok());
    POKO_CHECK(provider.asset_state(out2.asset) == poko::AssetState::Unknown);
    POKO_CHECK(!provider.get_asset(out2.asset).ok());
}

POKO_TEST_MAIN()
