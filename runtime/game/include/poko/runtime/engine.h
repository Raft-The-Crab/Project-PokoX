#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "poko/core/api_seams.h"
#include "poko/core/result.h"

namespace poko {
class CustomPhysicsWorld;
class FilesystemAssetProvider;
class MixerAudioDevice;
class ChannelNetworkTransport;
class LocalFileSystem;
class Renderer;
namespace graphics {
class MeshLibrary;
class MaterialLibrary;
class DrawCallBuilder;
}
} // namespace poko

namespace poko::runtime {

class GameRuntime;

// Which render backend the Engine should select. Only Null is constructible
// headlessly; OpenGL/Vulkan need a window/context and must be injected via
// set_render_device(). Auto walks the documented preference chain and lands on
// the last available backend (systemplan/04 step 10, systemplan/29).
enum class RenderBackend : std::uint8_t {
    Auto = 0,
    Null,
    OpenGL,
    Vulkan,
};

struct EngineDesc {
    double fixed_step_seconds = 1.0 / 60.0;
    std::uint32_t max_steps_per_frame = 8;
    // Root for the filesystem asset provider. Empty -> a temp directory is
    // created and populated with the sample scene's asset.
    std::filesystem::path asset_root;
    RenderBackend render_backend = RenderBackend::Auto;
    bool load_demo_scene = true;
    bool enable_audio = true;
    bool enable_network = true;
    // Deterministic fault-injection hook for the resource-failure path.
    bool inject_resource_failure = false;
};

struct EngineStats {
    std::uint64_t frames = 0;
    std::uint64_t simulation_steps = 0;
    std::uint32_t draw_calls = 0;
    std::uint64_t triangles = 0;
    std::uint32_t device_recoveries = 0;
    std::uint32_t resource_failures = 0;
    std::uint32_t skipped_draw_items = 0;
    std::uint32_t asset_cache_size = 0;
    std::uint32_t audio_active_voices = 0;
    std::uint32_t physics_bodies = 0;
    std::uint32_t transport_framed = 0;
    std::string active_backend = "unknown";
    std::string backend_chain = "unknown";
};

// PRODUCTION engine facade (Phase 1 exit, systemplan/27). Owns the real
// backends, the renderer, the graphics layer and the frame loop behind one
// lifecycle: initialize -> tick/run -> shutdown. Device selection uses the
// documented fallback chain; device loss is recovered automatically and GPU
// resource failures degrade to skipped draws instead of killing the loop.
//
// Only the render device may be the MOCK NullRenderDevice in headless/offline
// builds (no GPU available); every other subsystem is a PRODUCTION backend.
// The implementation is pimpl'd so this header stays free of backend types.
//
// Thread-safety contract: owned and driven by one frame thread.
class Engine {
public:
    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) noexcept;
    Engine& operator=(Engine&&) noexcept;

    Status initialize(const EngineDesc& desc);
    Status tick(double frame_seconds);
    Status run(std::uint32_t frames, double frame_seconds);
    Status shutdown();

    bool initialized() const noexcept;
    EngineStats stats() const;
    const char* active_backend_name() const noexcept;

    // Optional: supply a real render device before initialize(). Without one,
    // Auto selects the best available backend (Null in headless/offline builds).
    void set_render_device(std::unique_ptr<api::IRenderDevice> device) noexcept;

    // Deterministic fault-injection hooks for the systemplan/04 fallback path.
    void simulate_device_loss() noexcept;
    Status recover_device() noexcept;

    // Subsystem accessors. Valid only after initialize(); include the matching
    // module header to use the returned reference.
    CustomPhysicsWorld& physics() noexcept;
    FilesystemAssetProvider& assets() noexcept;
    MixerAudioDevice& audio() noexcept;
    ChannelNetworkTransport& network() noexcept;
    LocalFileSystem& filesystem() noexcept;
    Renderer& renderer() noexcept;
    graphics::MeshLibrary& meshes() noexcept;
    graphics::MaterialLibrary& materials() noexcept;
    graphics::DrawCallBuilder& draw_calls() noexcept;
    GameRuntime& runtime() noexcept;
    api::IRenderDevice& render_device() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace poko::runtime
