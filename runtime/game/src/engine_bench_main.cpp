#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "poko/assets/filesystem_provider.h"
#include "poko/audio/mixer_device.h"
#include "poko/benchmarks/benchmark.h"
#include "poko/benchmarks/registry.h"
#include "poko/benchmarks/runner.h"
#include "poko/core/asset_contracts.h"
#include "poko/core/audio_contracts.h"
#include "poko/core/net_contracts.h"
#include "poko/core/ownership.h"
#include "poko/core/physics_contracts.h"
#include "poko/network/channel_transport.h"
#include "poko/physics/custom_world.h"
#include "poko/platform/fs.h"
#include "poko/runtime/demo_scene.h"
#include "poko/runtime/game_runtime.h"

namespace {
constexpr std::uint32_t kIterations = 256;

struct PkxHeader {
    char magic[4];
    std::uint32_t kind;
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t c;
};

bool write_mesh_asset(const std::filesystem::path& root) {
    std::error_code ec;
    std::filesystem::create_directories(root / "models", ec);
    if (ec) {
        return false;
    }
    PkxHeader header{};
    std::memcpy(header.magic, "PKX1", 4);
    header.kind = static_cast<std::uint32_t>(poko::AssetKind::Mesh);
    header.a = 3;
    header.b = 3;
    header.c = 1;
    std::ofstream out(root / "models" / "demo.mesh", std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    const std::vector<char> payload(36, 0);
    out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    return out.good();
}

poko::BodyDesc make_body(std::uint32_t index) {
    poko::BodyDesc desc;
    desc.type = poko::BodyType::Dynamic;
    desc.shape.kind = poko::ShapeKind::Sphere;
    desc.shape.radius = 0.5f;
    desc.position = {static_cast<float>(index % 4) * 1.5f, 2.0f,
                     static_cast<float>(index / 4) * 1.5f};
    desc.mass = 1.0f;
    return desc;
}

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "engine baseline: " << message << '\n';
    }
    return condition;
}
} // namespace

int main() {
    try {
        const auto root = std::filesystem::temp_directory_path() / "pokox_engine_bench";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        poko::ScopeExit cleanup([root] { std::filesystem::remove_all(root); });
        if (!write_mesh_asset(root)) {
            std::cerr << "engine baseline: failed to prepare the benchmark asset\n";
            return 1;
        }

        poko::LocalFileSystem fs(root);
        poko::FilesystemAssetProvider assets(fs);
        poko::CustomPhysicsWorld physics;
        poko::MixerAudioDevice audio;
        poko::ChannelNetworkTransport network;
        if (!assets.initialize() || !physics.initialize(poko::PhysicsWorldDesc{}) ||
            !audio.initialize()) {
            std::cerr << "engine baseline: backend initialization failed\n";
            return 1;
        }

        for (std::uint32_t i = 0; i < 16; ++i) {
            (void)physics.create_body(make_body(i));
        }

        poko::SoundDesc ambient;
        ambient.kind = poko::SoundKind::Ambient;
        ambient.logical_path = "audio/bench.pcm";
        ambient.loop = true;
        const auto sound = audio.load_sound(ambient);
        if (!sound.ok() || !audio.play(sound.value()).ok()) {
            std::cerr << "engine baseline: audio setup failed\n";
            return 1;
        }
        std::vector<float> audio_buffer(256, 0.0f);

        const auto listener = network.listen("127.0.0.1", 47101);
        const auto client = network.connect("127.0.0.1", 47101);
        if (!listener.ok() || !client.ok()) {
            std::cerr << "engine baseline: transport setup failed\n";
            return 1;
        }
        network.update();
        const std::array<std::byte, 4> packet{std::byte{1}, std::byte{2}, std::byte{3},
                                             std::byte{4}};
        const std::span<const std::byte> packet_bytes(packet.data(), packet.size());

        poko::runtime::GameRuntime runtime(physics, assets);
        if (!poko::runtime::build_demo_scene(runtime).ok()) {
            std::cerr << "engine baseline: demo scene setup failed\n";
            return 1;
        }

        poko::AssetDesc mesh_desc;
        mesh_desc.kind = poko::AssetKind::Mesh;
        mesh_desc.logical_path = "models/demo.mesh";

        poko::benchmarks::BenchmarkRegistry registry;
        const auto add = [&registry](std::string name, std::function<void()> body) {
            return registry.add(poko::benchmarks::BenchmarkCase{std::move(name),
                                                                std::move(body)});
        };

        if (!add("physics.step", [&physics] { physics.step(1.0f / 60.0f); }).ok() ||
            !add("assets.decode",
                 [&] {
                     assets.evict("models/demo.mesh");
                     const auto request = assets.request_load(mesh_desc);
                     if (!request.ok()) {
                         return;
                     }
                     assets.update();
                     poko::LoadResult result;
                     if (assets.poll_load(request.value(), result)) {
                         (void)assets.release_asset(result.asset);
                     }
                 })
                 .ok() ||
            !add("audio.mix",
                 [&] { (void)audio.render(audio_buffer, 48000u); }).ok() ||
            !add("network.frame",
                 [&] {
                     (void)network.send(client.value(), poko::ChannelReliableOrdered,
                                        packet_bytes);
                     network.update();
                     (void)network.receive(
                         [](const poko::PacketDesc&, std::span<const std::byte>) {});
                 })
                 .ok() ||
            !add("runtime.tick",
                 [&runtime] { (void)runtime.tick(1.0 / 60.0); }).ok()) {
            std::cerr << "engine baseline: failed to register benchmark cases\n";
            return 1;
        }

        poko::benchmarks::BenchmarkRunner runner(registry);
        const auto results = runner.run_all(kIterations);

        std::cout << "engine performance baseline (advisory; no CI thresholds)\n";
        std::cout << "case,iterations,total_us,min_us,max_us,mean_us,ops_per_second\n";
        for (const auto& result : results) {
            std::cout << result.name << ',' << result.iterations << ',' << result.total_us << ','
                      << result.min_us << ',' << result.max_us << ',' << result.mean_us << ','
                      << result.ops_per_second << '\n';
        }

        bool ok = true;
        ok &= expect(results.size() == 5, "expected five benchmark cases");
        for (const auto& result : results) {
            ok &= expect(!result.name.empty(), "empty benchmark name");
            ok &= expect(result.iterations == kIterations, "iteration count mismatch");
            ok &= expect(result.total_us >= 0, "negative total time");
            ok &= expect(result.min_us <= result.max_us, "min exceeds max");
            ok &= expect(result.mean_us >= 0.0, "negative mean time");
        }
        ok &= expect(runtime.frame_index() == kIterations, "runtime tick count mismatch");
        ok &= expect(physics.body_count() == 17, "physics body count mismatch");

        std::cout << "engine baseline: " << (ok ? "OK" : "FAILED") << '\n';
        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "engine baseline: exception: " << error.what() << '\n';
        return 1;
    }
}
