#pragma once

// Early interface seams from systemplan/01_SYSTEM_DEPENDENCY_MAP.md.
//
// Phase 0 declared these as boundary placeholders (STUB). Phase 1 fills in the
// signatures owned by engine core and the renderer. Interfaces owned by later
// subsystems remain STUB declarations with only a virtual destructor. No seam
// may pull graphics, backend or UI vendor dependencies into engine core; the
// render contracts below are POD-only descriptors.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

#include "poko_asset_contracts.h"
#include "poko_audio_contracts.h"
#include "poko_physics_contracts.h"
#include "poko_render_contracts.h"
#include "poko_result.h"

namespace poko::api {

// Phase 1 (engine core): implemented by poko::LocalFileSystem.
class IPlatformFileSystem {
public:
    virtual ~IPlatformFileSystem() = default;
    virtual bool exists(std::string_view path) const = 0;
    virtual bool read_file(std::string_view path, std::string& out) const = 0;
    virtual std::uintmax_t file_size(std::string_view path) const = 0;
    virtual std::string absolute(std::string_view path) const = 0;
};

// Phase 1 (engine core): implemented by poko::JobSystem.
class IJobSystem {
public:
    virtual ~IJobSystem() = default;
    virtual void submit(std::function<void()> job) = 0;
    virtual void wait_idle() = 0;
};

// Phase 1 (renderer): the render-device boundary. Concrete backends live in
// engine/renderer/; a MOCK NullRenderDevice implements it for offline tests.
// Engine core only sees these signatures, never a graphics vendor.
class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    virtual const char* name() const noexcept = 0;
    virtual GraphicsBackend backend() const noexcept = 0;
    virtual DeviceCapabilities capabilities() const noexcept = 0;
    virtual bool available() const noexcept = 0;

    virtual Result<RenderHandle> create_buffer(const BufferDesc& desc) = 0;
    virtual Result<RenderHandle> create_texture(const TextureDesc& desc) = 0;
    virtual Result<RenderHandle> create_shader(const ShaderDesc& desc) = 0;
    virtual Result<RenderHandle> create_pipeline(const PipelineDesc& desc) = 0;
    virtual Result<RenderHandle> create_sampler(const SamplerDesc& desc) = 0;
    virtual Status destroy(RenderHandle handle) = 0;

    // Uploads raw bytes into a buffer created with create_buffer(). Backends
    // must reject writes past the buffer's declared size.
    virtual Status update_buffer(RenderHandle handle, std::span<const std::byte> data,
                                 std::uint64_t offset_bytes = 0) = 0;

    // Uploads one mip level of a texture created with create_texture(). The
    // data span must cover the full level (level dimensions x bytes per
    // pixel); partial uploads are not part of the seam yet.
    virtual Status update_texture(RenderHandle handle, std::uint32_t mip_level,
                                  std::span<const std::byte> data) = 0;

    virtual Status begin_frame() = 0;
    virtual Status clear(const ClearValue& value) = 0;
    virtual Status draw(const DrawCall& call) = 0;
    virtual Status end_frame() = 0;
    virtual void wait_idle() = 0;

    // systemplan/04 step 10: fallback, device-loss and resource failure.
    virtual bool device_lost() const noexcept = 0;
    virtual Status recover_from_device_loss() = 0;
};

// Phase 1 (physics, systemplan/06): rigid-body world boundary. The custom
// physics module is planned; a MOCK NullPhysicsWorld implements it for
// offline tests. Backend selection (custom vs library) is an open decision.
class IPhysicsWorld {
public:
    virtual ~IPhysicsWorld() = default;

    // Configures gravity and the fixed timestep. Must be called before use.
    virtual bool initialize(const PhysicsWorldDesc& desc) noexcept = 0;

    virtual Result<BodyHandle> create_body(const BodyDesc& desc) = 0;
    virtual Status destroy_body(BodyHandle body) = 0;

    // Teleports/updates a body (kinematic control, spawn correction).
    virtual Status set_body_state(BodyHandle body, const BodyState& state) = 0;
    virtual Result<BodyState> get_body_state(BodyHandle body) const = 0;

    // Returns true and fills `out` on the nearest hit within max_distance
    // whose layer/mask pair collides with layer_mask. `out` is untouched on
    // miss. Ray direction should be normalized.
    virtual bool raycast(const Ray& ray, float max_distance, std::uint32_t layer_mask,
                         RayHit& out) const = 0;

    // Advances simulation by delta_seconds of wall time; backends may
    // sub-step at PhysicsWorldDesc::fixed_delta_seconds internally.
    virtual void step(float delta_seconds) = 0;

    virtual std::uint32_t body_count() const noexcept = 0;
};

// Phase 1 (assets, systemplan/05): asynchronous asset loading boundary.
// logical_path is a package-relative path, never a raw OS path. Backends own
// decoding/caching; a MOCK NullAssetProvider implements it for offline tests.
class IAssetProvider {
public:
    virtual ~IAssetProvider() = default;

    virtual bool initialize() noexcept = 0;

    // Queues an async load. Returns a request id used with poll_load/cancel.
    virtual Result<LoadRequestId> request_load(const AssetDesc& desc) = 0;
    virtual Status cancel_load(LoadRequestId id) = 0;

    // Returns true and fills `out` once the request finished (Ready or
    // Failed). `out` is untouched while pending.
    virtual bool poll_load(LoadRequestId id, LoadResult& out) = 0;

    virtual AssetState asset_state(AssetHandle asset) const = 0;

    // Returns the completed payload for a Ready asset; fails otherwise.
    virtual Result<LoadResult> get_asset(AssetHandle asset) const = 0;

    // Drops the asset (eviction/lifetime rule). Pending loads are cancelled.
    virtual Status release_asset(AssetHandle asset) = 0;

    // Pumps queued work; backends may complete loads here or on workers.
    virtual void update() = 0;
};

// Phase 1 (audio, systemplan/07): playback device boundary. logical_path is
// package-relative. A MOCK NullAudioDevice implements it for offline tests;
// miniaudio is the planned production backend.
class IAudioDevice {
public:
    virtual ~IAudioDevice() = default;

    virtual bool initialize() noexcept = 0;

    // Loads/decodes a sound asset and returns a playable handle.
    virtual Result<SoundHandle> load_sound(const SoundDesc& desc) = 0;
    virtual Status unload_sound(SoundHandle sound) = 0;

    virtual Status play(SoundHandle sound) = 0;
    virtual Status stop(SoundHandle sound) = 0;
    virtual Status set_volume(SoundHandle sound, float volume) = 0;
    virtual Status set_paused(SoundHandle sound, bool paused) = 0;

    virtual bool is_playing(SoundHandle sound) const = 0;

    // Listener pose for positional attenuation.
    virtual Status set_listener(const ListenerState& listener) = 0;

    virtual AudioStats stats() const = 0;
};

class INetworkTransport {
public:
    virtual ~INetworkTransport() = default;
};

class IScriptRuntime {
public:
    virtual ~IScriptRuntime() = default;
};

class IPlatformServices {
public:
    virtual ~IPlatformServices() = default;
};

class IAccountProvider {
public:
    virtual ~IAccountProvider() = default;
};

class IGameCatalog {
public:
    virtual ~IGameCatalog() = default;
};

class IPackageStore {
public:
    virtual ~IPackageStore() = default;
};

class IServerRegistry {
public:
    virtual ~IServerRegistry() = default;
};

} // namespace poko::api
