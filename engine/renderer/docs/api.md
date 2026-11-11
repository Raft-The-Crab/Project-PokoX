# Renderer API reference (Phase 1 foundations)

**Status:** implemented and unit-tested. Covers the GPU-independent part of
`systemplan/04` (renderer and graphics). No graphics library is linked; the
plan's build sequence is mapped to the pieces that can exist offline:

| `systemplan/04` step | Delivered here |
|---|---|
| 1 Window/surface and device | `IRenderDevice` seam + `NullRenderDevice` (MOCK); real window blocked on SDL2 pinning |
| 2 Clear screen and triangle | `IRenderDevice::clear`, `DrawCall`, console render demo |
| 3 Shader compilation/cache and pipelines | `ShaderCache`, `PipelineDesc`, `create_pipeline` |
| 4 Vertex/index buffers and meshes | `BufferDesc`, `VertexLayout`, handles |
| 5 Textures and materials | `TextureDesc`, `PixelFormat`, handles |
| 6 Camera/transforms | `Camera` (view, projection, view-projection) |
| 7 Depth and basic lighting | depth/compare fields in `PipelineDesc` (no shading yet) |
| 8 Scene submission/batching | `SceneSubmission` |
| 9 GPU timing and diagnostics | `FrameStats` + dev-only `dump_to_log()` |
| 10 Fallback, device-loss, resource failure | `preferred/fallback/last_resort_backend()`, `simulate_device_loss()`, `recover_from_device_loss()`, boundary validation |

## Libraries

| Target | Label | Contents |
|---|---|---|
| `poko_renderer` | `PRODUCTION` | shader cache, camera, submission, frame stats, `Renderer` facade |
| `poko_render_null` | `MOCK` | `NullRenderDevice` — test-only fake, deliberately separate |

`poko_render_contracts.h` lives in `engine/core/include/` because
`IRenderDevice` is a dependency-map seam (`systemplan/01`). It contains only
POD descriptors and opaque handles — no vendor dependency. `poko_api_seams.h`
declares the interface; `engine/renderer/` implements it.

Excluded from the headless `Server` configuration (`systemplan/18`).

## Contracts

- Opaque `RenderHandle` = generation-checked `TypedHandle`. Destroying a
  resource bumps the generation so stale handles cannot alias a new object.
- All descriptors are validated at the boundary (`valid(...)` free functions).
  Invalid input returns `ErrorCode::InvalidArgument` before any allocation.
- `Status` / `Result<RenderHandle>` follow `poko_result.h`. Never call
  `value()` without checking `ok()`.
- Coordinate system: right-handed, +Y up, -Z forward, row-major matrices,
  OpenGL-style `[-1, 1]` clip depth. A future Vulkan backend applies its own
  depth-range conversion without changing `Camera`.

## IRenderDevice

The full signature is in `engine/core/include/poko_api_seams.h`. Key points:

- `create_buffer/texture/shader/pipeline` return `Result<RenderHandle>`.
- `destroy` returns `Status`; `begin_frame/clear/draw/end_frame` return
  `Status`.
- `wait_idle()`, `device_lost()`, `recover_from_device_loss()` support the
  fallback path of step 10.
- `capabilities()` reports backend, limits and supported features.

Backend order: **Vulkan -> OpenGL -> Null** (`preferred_backend()`,
`fallback_backend()`, `last_resort_backend()`), per `systemplan/29`.

## NullRenderDevice (MOCK)

A test-only fake. It allocates generation-checked handles, records clears and
draws for inspection, enforces frame lifecycle, and can simulate device loss.
`recover_from_device_loss()` discards in-flight commands but keeps handles.
Inspection hooks (`recorded_clears`, `recorded_draws`, `simulate_device_loss`)
are deliberately **outside** `IRenderDevice` so production code cannot depend
on them.

## CPU-side modules

- **`ShaderCache`** — deterministic FNV-1a key over stage + entry point +
  source; `get_or_compile` returns a cached `Entry` or compiles a MOCK
  artifact. Tracks compile/hit counts. Thread-safe.
- **`Camera`** — `view_matrix()` (look-at), `projection_matrix()`
  (perspective or orthographic), `view_projection()`. Invalid parameters fall
  back to safe defaults instead of producing NaN. Not thread-safe; owned by one
  renderer.
- **`SceneSubmission`** — `submit`, stable `batch()` (sort by pipeline then
  sort key, merge identical adjacent draws into instances), `batch_count()`,
  `total_triangles()`. Not thread-safe.
- **`FrameStats`** — per-frame CPU time, draw calls, triangles, GPU time.
  `dump_to_log()` is compiled only when `POKO_DEV_TOOLS` is defined (Debug and
  Development) so debug tooling cannot leak into Release/Server. Not
  thread-safe.
- **`Renderer`** — owns an injected `IRenderDevice`, wires the above together,
  and `render_frame()` clears, submits the batched draw list and ends the frame.

## Backends

- **`poko_render_null` [MOCK]** — `NullRenderDevice` records commands and
  simulates device loss; used by all default builds and CI.
- **`poko_render_gl` [PRODUCTION]** — `GlRenderDevice`, OpenGL 3.3 core.
  Built only with `POKO_RENDER_OPENGL=ON` (requires
  `POKO_FETCH_DEPENDENCIES=ON`). Renders to the current default framebuffer;
  the window layer owns context creation and swaps. See `DEC-004`.
- `IRenderDevice::update_buffer(handle, data, offset)` uploads raw bytes into
  a buffer; backends reject out-of-range writes.

## What is NOT here

Vulkan/Diligent backends (dependencies still unpinned), uniforms/UBO and
per-draw resource bindings, texture pixel upload, compiled shader bytecode,
lighting/shading, GPU timing queries, and the Dear ImGui debug panel
(`systemplan/09`). See `third_party/DEPENDENCIES.md`, `DEC-003` and `DEC-004`.
In offline builds `poko_windowed_demo` remains a labelled `STUB`;
`poko_render_demo` is the console test scene.

## Tests

`tests/unit/renderer_*.cpp` cover contracts, the MOCK device (including device
loss/recovery and stale handles), shader cache determinism, camera math,
batching and frame stats/facade. Run with `ctest --preset <preset>`.
