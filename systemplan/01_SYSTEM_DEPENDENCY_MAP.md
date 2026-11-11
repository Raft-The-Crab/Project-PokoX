# 01 — System Dependency Map

```text
Build / CI / Platform Abstraction
              |
          Engine Core
       /      |       \\
 Renderer  Physics   Resources
       \\      |       /
     Scene / World / Animation
              |
       Networking Layer
              |
       MUTE Interpreter
              |
       Game Runtime APIs
              |
       Package / Build System
              |
      Backend Service Contracts
              |
       Backend Implementations
              |
   Server Registry / Dedicated Host
              |
 Publishing / Review / Moderation
              |
 Social / Economy / Marketplace / Support
              |
     End-to-end Integration
              |
        Final Application UI
```

This is the major focus order, not a ban on parallel work. Teams can parallelize inside a subsystem after interfaces stabilize.

## Dependency rules
- Engine core must not depend on Cloudflare, Firebase, databases or UI.
- Backend must not depend on Dear ImGui.
- MUTE must not access arbitrary OS APIs.
- Game packages must not contain secrets.
- Untrusted games run in isolated environments.
- Final UI consumes stable application APIs and does not own business rules.

## Early interface seams
`IPlatformFileSystem`, `IJobSystem`, `IRenderDevice`, `IAssetProvider`, `IPhysicsWorld`, `IAudioDevice`, `INetworkTransport`, `IScriptRuntime`, `IPlatformServices`, `IAccountProvider`, `IGameCatalog`, `IPackageStore`, `IServerRegistry`.
