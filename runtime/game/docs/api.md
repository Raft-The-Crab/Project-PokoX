# runtime/game — integrated game runtime and engine facade

Phase 1 exit deliverable (systemplan/27 "stable local demo scene, documented
APIs, headless/windowed harnesses and performance baseline"). This wires the
breadth-first engine skeletons into one deterministic fixed-step frame loop, a
production engine facade, a sample scene running on real backends, and a
measured performance baseline.

Public headers live under `include/poko/runtime/`; sources under `src/`; tests
under `tests/`.

## Layout

- `include/poko/runtime/frame_context.h` — `RenderItem`, `FrameContext`,
  `RenderFn`. Render items carry opaque mesh/material IDs (not GPU handles), so
  the simulation runtime stays free of graphics types.
- `include/poko/runtime/game_runtime.h` — `GameRuntime` + `RuntimeDesc`.
- `include/poko/runtime/demo_scene.h` — `build_demo_scene()`.
- `include/poko/runtime/engine.h` — `Engine`, `EngineDesc`, `EngineStats`,
  `RenderBackend`.
- `src/game_runtime.cpp` — the backend-neutral frame loop.
- `src/demo_scene.cpp` — deterministic sample scene construction.
- `src/engine.cpp` — Engine lifecycle, tick/run/shutdown, stats, accessors.
- `src/engine_backends.cpp` — backend construction and render-device selection.
- `src/engine_render.cpp` — resource upload and the RenderFn (recovery +
  failure handling).
- `src/engine_impl.h` — private `Engine::Impl` plus shared `detail::` helpers.
- `src/headless_main.cpp` — `poko_headless`, the core-only self-check.
- `src/game_main.cpp` — `poko_game_demo`, the MOCK-backed seam regression.
- `src/engine_demo_main.cpp` — `poko_engine_demo`, the PRODUCTION demo scene.
- `src/engine_bench_main.cpp` — `poko_engine_bench`, the performance baseline.
- `tests/` — module-local unit and integration tests.

## Frame loop

`GameRuntime::tick(frame_seconds)` uses `poko::FixedTimestep` to derive N fixed
simulation steps. Each step rotates input edges, steps the injected
`api::IPhysicsWorld`, advances the `CharacterMotor` (flat-ground probe) and a
particle `Emitter`, and pumps the injected `api::IAssetProvider`. It then builds
a `FrameContext` (camera pose from `CameraRig`, light/post-process counts, render
items) and invokes the injected `RenderFn` exactly once. No wall clock is read,
so identical inputs produce identical output.

`RenderFn` is the presentation seam: a null callback makes the runtime
simulation-only, which is why `poko_runtime` has no graphics dependency and
builds in the headless Server configuration as well.

## Production engine facade (`poko_engine`, non-Server only)

`poko::runtime::Engine` is the Phase 1 entry point: one
`initialize -> tick/run -> shutdown` lifecycle owning the **PRODUCTION**
backends (`CustomPhysicsWorld`, `FilesystemAssetProvider`, `MixerAudioDevice`,
`ChannelNetworkTransport`, `LocalFileSystem`), the `Renderer`, the graphics
libraries and `GameRuntime`. It is pimpl'd so the public header stays free of
backend types; accessors return references (include the matching module header
to use them).

`initialize()` validates the descriptor, materializes the sample scene's real
`PKX1` mesh asset under `asset_root`, brings up every backend, selects the
render device, uploads mesh/material resources and builds the demo scene.
`tick()` mixes one audio block, pumps one framed network packet and advances
one frame. `stats()` aggregates frame/draw counts and per-backend counters.

Only the render device may be the MOCK `NullRenderDevice` in headless/offline
builds: `RenderBackend::Auto` walks the documented preference chain
(`vulkan -> opengl -> null` from `preferred_backend()`/`fallback_backend()`/
`last_resort_backend()`) and lands on the last available entry, since GL/VK
require a window/context. A real device is supplied with
`set_render_device()`; requesting OpenGL/Vulkan without one returns
`ErrorCode::Unsupported`. This is exactly the systemplan/04 step-10 fallback
chain, not a MOCK subsystem standing in for a production one.

### Failure handling (systemplan/04 exit gate)

- **Device loss** — the RenderFn checks `IRenderDevice::device_lost()` each
  frame and calls `recover_from_device_loss()` automatically, counting
  recoveries. A failed recovery degrades to a skipped frame rather than
  aborting simulation. `simulate_device_loss()` / `recover_device()` are
  deterministic test hooks.
- **Resource failures** — a failed mesh/material upload is counted, the
  affected draw items are skipped, and the loop keeps running. A failed
  `Renderer::render_frame` is likewise counted but non-fatal.
  `EngineDesc::inject_resource_failure` is the deterministic fault-injection
  hook.
- Tests cover both paths in `tests/engine_test.cpp`.

## Harnesses

- `poko_engine_demo` (`runtime.engine`) — the stable local demo scene on real
  backends: 60 deterministic frames, 2 batched draws/frame, real asset decode,
  one physics body, one ambient audio voice, one framed packet per tick.
- `poko_engine_bench` (`benchmark.engine`) — the Phase 1 performance baseline.
  Uses `poko_benchmarks` to measure `physics.step`, `assets.decode`,
  `audio.mix`, `network.frame` and `runtime.tick` over 256 iterations and
  prints CSV (total/min/max/mean µs, ops/sec). Builds in **every**
  configuration because it uses only the PRODUCTION CPU backends. Timings are
  advisory — no hard CI thresholds are asserted, only structural invariants.
- `poko_game_demo` (`runtime.game`) — retained MOCK-backed seam regression.
- `poko_headless` (`runtime.headless`) — core-only self-check.

A simulation-only dedicated-server runtime (systemplan/18) is later work; the
Server configuration builds `poko_runtime`, `poko_headless` and the baseline,
but no renderer/graphics targets.
