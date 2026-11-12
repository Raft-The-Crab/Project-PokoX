#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>

#include "poko/core/ownership.h"
#include "poko/runtime/engine.h"

namespace {
constexpr std::uint32_t kFrames = 60;
constexpr double kFrameSeconds = 1.0 / 60.0;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "engine demo: " << message << '\n';
    }
    return condition;
}
} // namespace

int main() {
    try {
        const auto root = std::filesystem::temp_directory_path() / "pokox_engine_demo";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        poko::ScopeExit cleanup([root] { std::filesystem::remove_all(root); });

        poko::runtime::EngineDesc desc;
        desc.asset_root = root;
        poko::runtime::Engine engine;
        if (!engine.initialize(desc).ok()) {
            std::cerr << "engine demo: initialization failed\n";
            return 1;
        }
        if (!engine.run(kFrames, kFrameSeconds).ok()) {
            std::cerr << "engine demo: frame loop failed\n";
            return 1;
        }

        const auto stats = engine.stats();
        std::cout << "engine demo: backend=" << stats.active_backend
                  << " chain=[" << stats.backend_chain << "]"
                  << " frames=" << stats.frames
                  << " sim_steps=" << stats.simulation_steps
                  << " draws/frame=" << stats.draw_calls
                  << " triangles/frame=" << stats.triangles
                  << " physics_bodies=" << stats.physics_bodies
                  << " asset_cache=" << stats.asset_cache_size
                  << " audio_voices=" << stats.audio_active_voices
                  << " transport_frames=" << stats.transport_framed
                  << " device_recoveries=" << stats.device_recoveries
                  << " resource_failures=" << stats.resource_failures << '\n';

        bool ok = true;
        ok &= expect(stats.frames == kFrames, "frame count mismatch");
        ok &= expect(stats.draw_calls == 2, "batched draw count mismatch");
        ok &= expect(stats.physics_bodies == 1, "physics body count mismatch");
        ok &= expect(stats.asset_cache_size == 1, "asset cache mismatch");
        ok &= expect(stats.audio_active_voices > 0, "no active audio voices");
        ok &= expect(stats.transport_framed == kFrames, "transport frame count mismatch");
        ok &= expect(stats.resource_failures == 0, "unexpected resource failures");
        ok &= expect(engine.shutdown().ok(), "shutdown failed");

        std::cout << "engine demo: " << (ok ? "OK" : "FAILED") << '\n';
        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "engine demo: exception: " << error.what() << '\n';
        return 1;
    }
}
