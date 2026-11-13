# Changelog

All notable changes to PokoX are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

PokoX is in the planning/foundation stage. Releases follow the staged roadmap
in `masterplan/23_ALPHA_BETA_AND_RELEASE_ROADMAP.md`: Stage 0 Foundations ->
Stage 1 Engine/client prototype -> Stage 2 Platform Alpha -> Stage 3 Closed
Beta -> Stage 4 Public launch -> Stage 5 Growth.

## [Unreleased]

### Added

- `poko_physics_custom`: input validation, speed and push-out limits, backlog cap,
  kinematic wake-up; exact (GJK time-of-impact) continuous collision detection including rotation, joints (ball, fixed, hinge, slider with limits and
  motors), contact warm starting and rest damping; exact sphere/capsule/box narrow phase, sweep-and-prune
  broadphase, sequential-impulse solver with inertia and friction, island
  sleeping and oriented raycasts.
- `poko_ecs`: sparse-set pools, multi-component `each`/`entities_with` queries,
  dead-entity safety.
- `poko_render_software`: CPU rasterizer render device (depth, culling,
  clipping, lit and textured shading, PPM readback), plus graphics primitives,
  `SceneRenderer` and the `poko_physics_showcase` demo.
- `poko_replication`: PKXR delta-snapshot replication with interest management,
  server-authoritative input validation and client interpolation over the
  transport seam.

- Initial repository foundation (Stage 0).
- Proprietary `LICENSE` establishing closed-source terms.
- `CONTRIBUTING.md` covering the system-first development philosophy,
  engineering rules, technology stack and mandatory git rules.
- `CODEBUDDY.md` capturing the standing repository rules (push only on explicit
  instruction, single git identity, no AI attribution in commits).
- `.gitignore` for the C++/Rust/Kotlin/TypeScript/Go multi-language toolchain.
- `NOTICE` for attribution and third-party notices.
- Full source-tree scaffold: `engine/`, `runtime/`, `applications/`,
  `backend/`, `server/`, `infrastructure/`, `tools/`, `assets/`, `packages/`,
  `sdk/`, `docs/`, `tests/`, `third_party/`, `scripts/`, `config/`,
  `profiles/`, `ci/`, `examples/`, `benchmarks/`, `ops/`, `security/`,
  `localization/`, `labs/` and `.github/`.
- Stage 0 / Phase 0 build foundation: root `CMakeLists.txt` (C++20),
  `CMakePresets.json` and the `config/cmake/` modules with four distinct
  configurations (`Debug`, `Development`, `Release`, headless `Server`).
- `poko_core` foundation: version, build-config constants, stable IDs and
  handles, structured logging with a capturable sink, and assertions that
  compile away in release/server builds.
- `STUB` declarations of the early interface seams from `systemplan/01`.
- Buildable targets from `systemplan/02`: headless demo, windowed demo stub,
  `poko_assetc`, `poko_packagec`, `poko_verifier`, `poko_serverctl` and
  `poko_server`, all carrying explicit implementation labels.
- Dependency-free unit-test harness and four CTest-registered unit test
  executables covering version, types, logging and assertions.
- `.github/workflows/ci.yml` with a four-configuration build/test matrix on
  Windows and Ubuntu, an advisory formatting/static-analysis job, and a
  secrets policy check; `.clang-format` and `.clang-tidy` configurations.
- `scripts/build/verify_all.sh` for reproducible local verification.
- Design and register drafts: `docs/guides/building.md`,
  `docs/architecture/core_api_and_security_design.md`,
  `docs/architecture/package_manifest_draft.md`,
  `packages/schema/manifest.schema.json`,
  `third_party/DEPENDENCIES.md`, `third_party/licenses/INVENTORY.md` and
  `docs/decisions/0001-phase0-build-foundation.md`.
- Phase 1 engine core (`systemplan/03`): `poko_result.h` (standardized errors),
  `poko_math.h` (vectors, row-major matrices, quaternions with a documented
  right-handed coordinate system), `poko_clock.h` (steady-clock stopwatch and
  capped fixed timestep), `poko_event.h` (instance-based `EventBus`),
  `poko_job.h` (thread-pool `JobSystem`), `poko_diagnostics.h`,
  `poko_settings.h` and `poko_ownership.h` (counters/gauges/timers, typed
  configuration, `ObserverPtr`/`ScopeExit`/generation-checked
  `ResourceRegistry`).
- `LocalFileSystem` in `engine/platform_abstraction/` implementing
  `api::IPlatformFileSystem` with a root-scoped path sandbox; `IJobSystem` and
  `IPlatformFileSystem` now have concrete signatures.
- Five additional CTest unit-test executables covering result types, math,
  clocks/events/jobs, diagnostics/settings/ownership and the filesystem. The
  headless console demo now exercises all core subsystems.
- `engine/core/docs/api.md` documenting core APIs, ownership and thread-safety
  contracts, and `docs/decisions/0002-phase1-engine-core.md`.
- Phase 1 renderer foundations (`systemplan/04`): POD render contracts and a
  full `api::IRenderDevice` seam in `poko_api_seams.h`; `poko_renderer`
  (`ShaderCache`, `Camera`, `SceneSubmission`, `FrameStats`, `Renderer` facade)
  and a separate `poko_render_null` MOCK device with generation-checked
  handles, command recording, device-loss simulation and recovery. Backend
  order recorded as Vulkan -> OpenGL -> Null.
- `poko_render_demo` console test scene; renderer excluded from the headless
  `Server` configuration; `FrameStats::dump_to_log()` gated by
  `POKO_DEV_TOOLS`.
- Six renderer CTest unit-test executables covering contracts, the MOCK device,
  shader cache determinism, camera math, batching and frame stats/facade.
- `engine/renderer/docs/api.md`, `docs/decisions/0003-phase1-renderer-foundations.md`
  and a renderer note in `third_party/DEPENDENCIES.md`.
- SDL3 `release-3.4.20` pinned (SHA-256-verified codeload tarball, zlib) and
  fetched only when `POKO_FETCH_DEPENDENCIES=ON`; generated gl3w OpenGL loader
  sources vendored under `third_party/vendored/gl3w/` (The Unlicense) so builds
  stay offline (`docs/decisions/0004-sdl3-and-opengl-backend.md`).
- `poko_render_gl` [PRODUCTION]: OpenGL 3.3 core implementation of
  `api::IRenderDevice` — buffers with `update_buffer`, 2D textures, GLSL
  compile/link, pipeline state, clear/draw/frame lifecycle.
- `IRenderDevice::update_buffer` seam (raw byte upload with bounds checks);
  `poko_windowed_demo` is now a real SDL3 window rendering a triangle
  (`--frames N` for automated runs), still a labelled STUB in offline builds.
- clang-format 23.1.5 and clang-tidy 23.1.4 pinned; the CI formatting/static
  analysis job is now blocking and the tree is clang-format clean.
- Vulkan backend (`systemplan/04` primary): Vulkan-Headers, Vulkan-Loader and
  glslang pinned to `vulkan-sdk-1.4.363.0` (Apache-2.0/BSD-3-Clause), fetched
  only when `POKO_RENDER_VULKAN=ON`; `poko_render_vulkan` [PRODUCTION]
  implements `IRenderDevice` (swapchain, render pass + depth, frames in
  flight, glslang GLSL→SPIR-V, pipelines, draws) and `vulkan_demo` renders a
  triangle on a real VkSurfaceKHR (`docs/decisions/0005-vulkan-render-backend.md`).

### Changed

- Recorded `main` as the default branch in `CODEBUDDY.md` and
  `CONTRIBUTING.md`.

### Notes

- `systemplan/` and `masterplan/` hold the planning baseline (2026-11-11) and
  are not implementation code.
- All proposed product features are marked as proposals until confirmed by the
  project owner. No public release claims are made.

## [0.0.0] - 2026-11-11

### Added

- Project planning archive: 38 master-plan documents and 31 system-first
  engineering documents, plus `START_HERE.md`.
- `START_HERE.md` reading order and the confirmed achievement expansion rule
  (100 slots per game, +10 slots per 150 Pokoins, unlimited, non-transferable).

[Unreleased]: https://github.com/Raft-The-Crab/Project-PokoX/compare/v0.0.0...HEAD
[0.0.0]: https://github.com/Raft-The-Crab/Project-PokoX/releases/tag/v0.0.0
