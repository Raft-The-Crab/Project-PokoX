# runtime/game — integrated game runtime

Phase 1 exit deliverable (systemplan/27 "stable local demo scene"). This wires
the breadth-first engine skeletons into one deterministic fixed-step frame loop
and a sample scene, so the subsystems are exercised together instead of only in
isolation.

Public headers live under `include/poko/runtime/`; sources under `src/`; tests
under `tests/`.

## Layout

- `include/poko/runtime/frame_context.h` — `RenderItem`, `FrameContext`,
  `RenderFn`. Render items carry opaque mesh/material IDs (not GPU handles), so
  the simulation runtime stays free of graphics types.
- `include/poko/runtime/game_runtime.h` — `GameRuntime` + `RuntimeDesc`.
- `include/poko/runtime/demo_scene.h` — `build_demo_scene()`.
- `src/game_runtime.cpp` — the frame loop.
- `src/demo_scene.cpp` — deterministic sample scene construction.
- `src/headless_main.cpp` — the original core-only headless self-check
  (`poko_headless`), unchanged and available in every configuration.
- `src/game_main.cpp` — `poko_game_demo`, the client-side integration harness.
- `tests/runtime_frame_loop_test.cpp` — module-local unit tests.

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

## Client integration (`poko_game_demo`, non-Server only)

The demo constructs a `poko::Renderer` over the MOCK `NullRenderDevice`, builds
`MeshLibrary`/`MaterialLibrary`/`DrawCallBuilder` on `Renderer::device()`, and
installs a `RenderFn` that maps each `RenderItem` through `DrawCallBuilder` into
`SceneSubmission`, batches, and calls `Renderer::render_frame`. Two render items
share a sort key so the batcher merges them into one instanced draw. The harness
runs 60 deterministic frames and asserts the end-to-end counters (frame count,
batched draws, recorded device draws, GPU resources, uploads, scene population).

A simulation-only dedicated-server runtime (systemplan/18) is later work; this
integration targets the local demo scene.
