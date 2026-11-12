#include "poko_test_harness.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "poko/assets/filesystem_provider.h"
#include "poko/core/asset_contracts.h"
#include "poko/core/ownership.h"
#include "poko/platform/fs.h"

namespace {
using poko::AssetDesc;
using poko::AssetKind;
using poko::AssetState;
using poko::FilesystemAssetProvider;
using poko::LoadResult;
using poko::LocalFileSystem;
using poko::PixelFormat;

struct Header {
    char magic[4];
    std::uint32_t kind;
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t c;
};

void write_pkx(const std::filesystem::path& dir, const char* name, std::uint32_t kind,
               std::uint32_t a, std::uint32_t b, std::uint32_t c,
               std::size_t payload_bytes = 8, const char* magic = "PKX1") {
    Header header{};
    std::memcpy(header.magic, magic, 4);
    header.kind = kind;
    header.a = a;
    header.b = b;
    header.c = c;
    std::ofstream out(dir / name, std::ios::binary);
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    const std::vector<char> payload(payload_bytes, 0);
    out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
}

std::filesystem::path make_temp_dir(const char* tag) {
    const auto dir = std::filesystem::temp_directory_path() / ("pokox_assets_" + std::string(tag));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

AssetDesc desc(AssetKind kind, const char* path) {
    AssetDesc out;
    out.kind = kind;
    out.logical_path = path;
    out.debug_name = path;
    return out;
}
} // namespace

POKO_TEST(loads_and_decodes_real_files) {
    const auto dir = make_temp_dir("decode");
    poko::ScopeExit cleanup([dir] { std::filesystem::remove_all(dir); });
    write_pkx(dir, "mesh.pkx", static_cast<std::uint32_t>(AssetKind::Mesh), 3, 3, 1);
    write_pkx(dir, "tex.pkx", static_cast<std::uint32_t>(AssetKind::Texture), 4, 4,
              static_cast<std::uint32_t>(PixelFormat::RGBA8));
    write_pkx(dir, "sound.pkx", static_cast<std::uint32_t>(AssetKind::Audio), 44100, 2, 16, 1764);
    write_pkx(dir, "blob.pkx", static_cast<std::uint32_t>(AssetKind::Raw), 0, 0, 0, 32);

    LocalFileSystem fs(dir);
    FilesystemAssetProvider provider(fs);
    POKO_CHECK(provider.initialize());

    const auto mesh = provider.request_load(desc(AssetKind::Mesh, "mesh.pkx"));
    const auto tex = provider.request_load(desc(AssetKind::Texture, "tex.pkx"));
    const auto sound = provider.request_load(desc(AssetKind::Audio, "sound.pkx"));
    const auto blob = provider.request_load(desc(AssetKind::Raw, "blob.pkx"));
    POKO_CHECK(mesh.ok() && tex.ok() && sound.ok() && blob.ok());

    LoadResult result;
    POKO_CHECK(!provider.poll_load(mesh.value(), result)); // pending until update()
    provider.update();

    POKO_CHECK(provider.poll_load(mesh.value(), result));
    POKO_CHECK(result.state == AssetState::Ready);
    POKO_CHECK_EQ(result.payload.mesh.vertex_count, 3u);
    POKO_CHECK_EQ(result.payload.mesh.index_count, 3u);
    POKO_CHECK_EQ(result.payload.mesh.submesh_count, 1u);
    POKO_CHECK_EQ(result.payload.byte_size, std::uint64_t{8});

    POKO_CHECK(provider.poll_load(tex.value(), result));
    POKO_CHECK_EQ(result.payload.texture.width, 4u);
    POKO_CHECK(result.payload.texture.format == PixelFormat::RGBA8);

    POKO_CHECK(provider.poll_load(sound.value(), result));
    POKO_CHECK_EQ(result.payload.audio.sample_rate, 44100u);
    POKO_CHECK_EQ(result.payload.audio.channels, 2u);
    POKO_CHECK(result.payload.audio.duration_seconds > 0.0f);

    POKO_CHECK(provider.poll_load(blob.value(), result));
    POKO_CHECK_EQ(result.payload.byte_size, std::uint64_t{32});
    POKO_CHECK(!provider.poll_load(blob.value(), result)); // one-shot

    const auto got = provider.get_asset(result.asset);
    POKO_CHECK(got.ok());
    POKO_CHECK(provider.asset_state(result.asset) == AssetState::Ready);
    POKO_CHECK(provider.release_asset(result.asset).ok());
    POKO_CHECK(!provider.release_asset(result.asset).ok()); // stale handle
    POKO_CHECK(!provider.get_asset(result.asset).ok());
    POKO_CHECK_EQ(provider.failed_load_count(), 0u);
}

POKO_TEST(missing_and_malformed_files_fail) {
    const auto dir = make_temp_dir("fail");
    poko::ScopeExit cleanup([dir] { std::filesystem::remove_all(dir); });
    write_pkx(dir, "bad_magic.pkx", static_cast<std::uint32_t>(AssetKind::Mesh), 3, 3, 1, 8,
              "XXXX");
    write_pkx(dir, "wrong_kind.pkx", static_cast<std::uint32_t>(AssetKind::Texture), 3, 3, 1);

    LocalFileSystem fs(dir);
    FilesystemAssetProvider provider(fs);
    POKO_CHECK(provider.initialize());

    const auto missing = provider.request_load(desc(AssetKind::Mesh, "missing.pkx"));
    const auto bad_magic = provider.request_load(desc(AssetKind::Mesh, "bad_magic.pkx"));
    const auto wrong_kind = provider.request_load(desc(AssetKind::Mesh, "wrong_kind.pkx"));
    POKO_CHECK(missing.ok() && bad_magic.ok() && wrong_kind.ok());
    provider.update();

    LoadResult result;
    POKO_CHECK(provider.poll_load(missing.value(), result));
    POKO_CHECK(result.state == AssetState::Failed);
    POKO_CHECK(provider.poll_load(bad_magic.value(), result));
    POKO_CHECK(result.state == AssetState::Failed);
    POKO_CHECK(provider.poll_load(wrong_kind.value(), result));
    POKO_CHECK(result.state == AssetState::Failed);
    POKO_CHECK_EQ(provider.failed_load_count(), 3u);
}

POKO_TEST(cancel_before_update_prevents_load) {
    const auto dir = make_temp_dir("cancel");
    poko::ScopeExit cleanup([dir] { std::filesystem::remove_all(dir); });
    write_pkx(dir, "mesh.pkx", static_cast<std::uint32_t>(AssetKind::Mesh), 3, 3, 1);

    LocalFileSystem fs(dir);
    FilesystemAssetProvider provider(fs);
    POKO_CHECK(provider.initialize());
    const auto id = provider.request_load(desc(AssetKind::Mesh, "mesh.pkx"));
    POKO_CHECK(id.ok());
    POKO_CHECK(provider.cancel_load(id.value()).ok());
    POKO_CHECK(!provider.cancel_load(id.value()).ok());
    provider.update();
    LoadResult result;
    POKO_CHECK(!provider.poll_load(id.value(), result));
    POKO_CHECK_EQ(provider.failed_load_count(), 0u);
}

POKO_TEST(cache_serves_repeat_loads_and_evict_forces_reread) {
    const auto dir = make_temp_dir("cache");
    poko::ScopeExit cleanup([dir] { std::filesystem::remove_all(dir); });
    write_pkx(dir, "mesh.pkx", static_cast<std::uint32_t>(AssetKind::Mesh), 3, 3, 1);

    LocalFileSystem fs(dir);
    FilesystemAssetProvider provider(fs);
    POKO_CHECK(provider.initialize());
    const auto first = provider.request_load(desc(AssetKind::Mesh, "mesh.pkx"));
    POKO_CHECK(first.ok());
    provider.update();
    LoadResult result;
    POKO_CHECK(provider.poll_load(first.value(), result));
    POKO_CHECK(result.state == AssetState::Ready);
    POKO_CHECK_EQ(provider.cache_size(), 1u);

    // Delete the file: a cached load must still succeed without I/O.
    std::filesystem::remove(dir / "mesh.pkx");
    const auto second = provider.request_load(desc(AssetKind::Mesh, "mesh.pkx"));
    POKO_CHECK(second.ok());
    provider.update();
    POKO_CHECK(provider.poll_load(second.value(), result));
    POKO_CHECK(result.state == AssetState::Ready);
    POKO_CHECK_EQ(result.payload.mesh.vertex_count, 3u);
    POKO_CHECK_EQ(provider.failed_load_count(), 0u);

    // Eviction makes the next load really read (and fail, file is gone).
    provider.evict("mesh.pkx");
    const auto third = provider.request_load(desc(AssetKind::Mesh, "mesh.pkx"));
    POKO_CHECK(third.ok());
    provider.update();
    POKO_CHECK(provider.poll_load(third.value(), result));
    POKO_CHECK(result.state == AssetState::Failed);
    POKO_CHECK_EQ(provider.failed_load_count(), 1u);
}

POKO_TEST(validates_init_and_descriptors) {
    const auto dir = make_temp_dir("validate");
    poko::ScopeExit cleanup([dir] { std::filesystem::remove_all(dir); });
    LocalFileSystem fs(dir);
    FilesystemAssetProvider provider(fs);
    POKO_CHECK(!provider.request_load(desc(AssetKind::Mesh, "x.pkx")).ok()); // not initialized
    POKO_CHECK(provider.initialize());
    POKO_CHECK(!provider.request_load(AssetDesc{}).ok());
    POKO_CHECK(!provider.cancel_load(poko::kInvalidLoadRequest).ok());
}

POKO_TEST_MAIN()
