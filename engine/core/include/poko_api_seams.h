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
    virtual Status destroy(RenderHandle handle) = 0;

    // Uploads raw bytes into a buffer created with create_buffer(). Backends
    // must reject writes past the buffer's declared size.
    virtual Status update_buffer(RenderHandle handle, std::span<const std::byte> data,
                                 std::uint64_t offset_bytes = 0) = 0;

    virtual Status begin_frame() = 0;
    virtual Status clear(const ClearValue& value) = 0;
    virtual Status draw(const DrawCall& call) = 0;
    virtual Status end_frame() = 0;
    virtual void wait_idle() = 0;

    // systemplan/04 step 10: fallback, device-loss and resource failure.
    virtual bool device_lost() const noexcept = 0;
    virtual Status recover_from_device_loss() = 0;
};

// STUB declarations — owned by later subsystems (systemplan/01, 27).

class IAssetProvider {
public:
    virtual ~IAssetProvider() = default;
};

class IPhysicsWorld {
public:
    virtual ~IPhysicsWorld() = default;
};

class IAudioDevice {
public:
    virtual ~IAudioDevice() = default;
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
