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
#include "poko/core/physics_contracts.h"
#include "poko/physics/custom_world.h"
#include "poko/platform/fs.h"
#include "poko/runtime/demo_scene.h"
#include "poko/runtime/game_runtime.h"

namespace {
using poko::AssetKind;
using poko::BodyDesc;
using poko::BodyType;
using poko::CustomPhysicsWorld;
using poko::FilesystemAssetProvider;
using poko::LocalFileSystem;
using poko::PhysicsWorldDesc;
using poko::ShapeKind;
using poko::runtime::GameRuntime;

struct Header {
    char magic[4];
    std::uint32_t kind;
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t c;
};

void write_mesh_file(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir / "models");
    Header header{};
    std::memcpy(header.magic, "PKX1", 4);
    header.kind = static_cast<std::uint32_t>(AssetKind::Mesh);
    header.a = 3; // vertex_count
    header.b = 3; // index_count
    header.c = 1; // submesh_count
    std::ofstream out(dir / "models" / "demo.mesh", std::ios::binary);
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    const std::vector<char> payload(36, 0);
    out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
}
} // namespace

POKO_TEST(real_backends_are_drop_in_behind_the_runtime_seams) {
    const auto dir =
        std::filesystem::temp_directory_path() / "pokox_runtime_real_backends";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    poko::ScopeExit cleanup([dir] { std::filesystem::remove_all(dir); });
    write_mesh_file(dir);

    LocalFileSystem fs(dir);
    FilesystemAssetProvider assets(fs);
    CustomPhysicsWorld physics;
    POKO_CHECK(assets.initialize());
    POKO_CHECK(physics.initialize(PhysicsWorldDesc{}));

    GameRuntime game(physics, assets);
    POKO_CHECK(poko::runtime::build_demo_scene(game).ok());

    // A directly-created probe body proves the PRODUCTION world is being
    // stepped with real gravity integration inside the runtime loop.
    BodyDesc probe;
    probe.type = BodyType::Dynamic;
    probe.shape.kind = ShapeKind::Sphere;
    probe.shape.radius = 0.5f;
    probe.position = {0.0f, 4.0f, 0.0f};
    probe.mass = 1.0f;
    const auto body = game.physics().create_body(probe);
    POKO_CHECK(body.ok());

    POKO_CHECK(game.run(60, 1.0 / 60.0).ok());
    POKO_CHECK_EQ(game.frame_index(), 60u);
    POKO_CHECK_EQ(game.simulation_step_count(), 60u);

    const auto state = physics.get_body_state(body.value());
    POKO_CHECK(state.ok());
    POKO_CHECK(state.value().position.y < 4.0f); // gravity was applied
    POKO_CHECK_EQ(physics.body_count(), 2u);     // demo body + probe

    // The demo's asset request was really read and decoded from disk.
    POKO_CHECK_EQ(assets.cache_size(), 1u);
    POKO_CHECK_EQ(assets.failed_load_count(), 0u);
}

POKO_TEST_MAIN()
