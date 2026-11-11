# DEC-003 — Phase 1 renderer foundations

- **Date:** 2026-11-12
- **Status:** Accepted
- **Owner:** Raft-The-Crab
- **Category:** Engine/runtime

## Context

`systemplan/27` Phase 1 continues after engine core with the renderer
(`systemplan/04`). The confirmed direction (`systemplan/29`) is Vulkan primary,
OpenGL fallback, Diligent Engine integration as selected. However `DEC-001`
pins no third-party dependency: SDL2, Vulkan, OpenGL loaders and Diligent are
all `NOT VENDORED` with unverified licences, and `POKO_FETCH_DEPENDENCIES`
defaults to `OFF` to keep clean-checkout builds offline and reproducible.

Vendoring a graphics stack now would break that policy and introduce unverified
licences. Skipping the renderer would break the sequential subsystem order.

## Options considered

1. **Vendor SDL2 + Vulkan immediately and open a window.** Fastest visible
   result, but violates `DEC-001`, requires licence/hash verification that has
   not happened, and cannot be reproduced offline.
2. **Build the GPU-independent renderer layer first with a labelled MOCK
   device.** Keeps the seam stable, makes the build sequence testable offline,
   and lets a real backend be swapped in later without changing upper-layer
   APIs.
3. **Skip to another subsystem.** Violates `systemplan/01` dependency order.

## Decision

Adopt option 2.

- `poko_render_contracts.h` (engine core): POD descriptors, formats, enums,
  `RenderHandle`, and `valid()` boundary checks. Core still depends on no
  graphics vendor.
- `poko_api_seams.h`: `IRenderDevice` receives its real signature. `IAssetProvider`,
  `IPhysicsWorld`, `IAudioDevice`, `INetworkTransport` and the rest remain STUB.
- `engine/renderer/`: two libraries — `poko_renderer` (PRODUCTION) with
  `ShaderCache`, `Camera`, `SceneSubmission`, `FrameStats` and the `Renderer`
  facade; and `poko_render_null` (MOCK) with `NullRenderDevice`. Splitting the
  MOCK into its own library makes it impossible to confuse with production
  (rule 12 of `systemplan/30`).
- Backend order recorded as Vulkan -> OpenGL -> Null. `NullRenderDevice`
  simulates device loss and recovery so step 10 of the build sequence is
  testable. `FrameStats::dump_to_log()` is gated by `POKO_DEV_TOOLS`.
- `poko_render_demo` is the console test scene; `poko_windowed_demo` stays a
  STUB. The renderer is excluded from the headless `Server` configuration.

## Consequences

- No window or triangle is drawn yet. The real SDL2/Vulkan/OpenGL/Diligent
  backends, compiled shader bytecode, texture upload, lighting and the Dear
  ImGui GPU panel (`systemplan/09`) are deferred until dependencies are pinned.
- A MOCK device ships in the build tree but is clearly labelled and never
  selected implicitly: `Renderer` requires an injected device.
- Camera projection uses OpenGL-style `[-1, 1]` clip depth; a Vulkan backend
  applies its own conversion behind the same API.
- Tests remain conditional on `TARGET poko_renderer` so the Server build stays
  valid.

## Security/privacy review

All descriptors are validated at the module boundary before allocation. No
untrusted shader source is executed (the cache stores a MOCK artifact). No
network, secrets or production credentials involved. Device-loss recovery
discards in-flight commands.

## Migration plan

Before the next renderer increment: pin SDL2, Vulkan headers/loader, an OpenGL
loader and Diligent Engine to immutable revisions with hashes; copy licence
texts into `third_party/licenses/`; add `FetchContent` entries behind
`POKO_FETCH_DEPENDENCIES`; implement `VulkanRenderDevice` and
`OpenGLRenderDevice` against `IRenderDevice`; replace the windowed STUB. Keep
`NullRenderDevice` as the MOCK for CI.

## Related documents

- `systemplan/01`, `04`, `09`, `18`, `27`, `29`, `30`
- `DEC-001`, `DEC-002`
- `engine/renderer/docs/api.md`
- `third_party/DEPENDENCIES.md`
