#pragma once

// Early interface seams from systemplan/01_SYSTEM_DEPENDENCY_MAP.md.
//
// Phase 0 declared these as boundary placeholders (STUB). Phase 1 starts
// filling in the signatures owned by engine core. Interfaces owned by later
// subsystems remain STUB declarations with only a virtual destructor. No seam
// may pull graphics, backend or UI dependencies into engine core.

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

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

// STUB declarations — owned by later subsystems (systemplan/01, 27).
class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;
};

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

}  // namespace poko::api
