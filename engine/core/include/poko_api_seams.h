#pragma once

// STUB — early interface seams from systemplan/01_SYSTEM_DEPENDENCY_MAP.md.
//
// These are boundary declarations ONLY. Concrete method signatures are
// specified when the owning subsystem is implemented (Phase 1 onwards), so
// that engine core never depends on graphics, backend or UI vendors. No
// Phase 0 target provides an implementation.

namespace poko::api {

class IPlatformFileSystem {
public:
    virtual ~IPlatformFileSystem() = default;
};

class IJobSystem {
public:
    virtual ~IJobSystem() = default;
};

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
