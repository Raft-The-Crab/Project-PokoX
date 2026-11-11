# DEC-005 — Vulkan render backend

- **Date:** 2026-11-12
- **Status:** Accepted
- **Owner:** Raft-The-Crab
- **Category:** Engine/runtime

## Context

`DEC-003`/`DEC-004` deferred Vulkan as the stated primary backend until
dependencies could be pinned. `systemplan/04` requires a real
`IRenderDevice` implementation; the OpenGL backend (`DEC-004`) proved the
window→device→draw path, and Vulkan is next in the recorded backend order.

## Options considered

1. **Raw Vulkan via Vulkan-Headers + Vulkan-Loader + glslang.** Full control,
   matches the `IRenderDevice` seam directly; most code to write.
2. **SDL3 GPU API (`SDL_GPU`).** Cross-API abstraction over Vulkan/D3D12/Metal;
   much less code, but on Windows it prefers D3D12, which conflicts with the
   "Vulkan primary" requirement unless driver hints are forced.
3. **Diligent Engine.** Deferred — still unpinned; revisit after the native
   backend exists.

## Decision

Adopt option 1, pinned to the `vulkan-sdk-1.4.363.0` tag (matching the
locally installed Vulkan SDK):

- **Vulkan-Headers** `6802bb4…` — Apache-2.0
- **Vulkan-Loader** `fa21805…` — Apache-2.0 (built shared `vulkan-1`)
- **glslang** `e1b562a…` — BSD-3-Clause; compiles `ShaderDesc` GLSL text to
  SPIR-V at `create_shader` time (`ENABLE_OPT=OFF`; SPIRV-Tools is not a
  pinned dependency)

All fetched via `FetchContent` only when `POKO_RENDER_VULKAN=ON` (which
requires `POKO_FETCH_DEPENDENCIES=ON` and SDL3 for `VkSurfaceKHR`).

`poko_render_vulkan` [PRODUCTION] implements `IRenderDevice`:

- Instance/physical-device selection is the window layer's job (the demo
  enumerates via SDL3 `SDL_Vulkan_*`); the device takes `VkInstance`,
  `VkPhysicalDevice`, `VkSurfaceKHR` as opaque pointers.
- Logical device + graphics queue, swapchain (FIFO, sRGB/BGRA preferred),
  render pass with color + `D32_SFLOAT` depth attachment per swap image.
- **Frames in flight**: command buffer + fence + acquire/render semaphores
  live per frame slot (count = swapchain image count), *not* per image —
  `vkAcquireNextImageKHR` can return any image, so the acquire semaphore
  must be chosen before the index is known. A per-image semaphore scheme
  deadlocks once an image index ≠ 0 is reused (found and fixed this way).
- Buffers are host-visible/coherent (simple `update_buffer` via
  `vkMapMemory`+memcpy); textures are 2D images + views; pipelines are
  `VkGraphicsPipeline` from `PipelineDesc` with dynamic viewport/scissor.
- `vulkan_demo` renders the same triangle as the GL demo (`--frames N`).

## Consequences / honest limitations

- Verified locally: `demo.vulkan` renders 60 frames on AMD Radeon
  (RADV/Vulkan), `ctest` 24/24 in the combined GL+Vulkan build.
- **No uniforms/UBO or descriptor sets yet** — same gap as GL; camera path
  is the next seam extension (push constants or UBO + descriptor).
- No texture pixel upload path (`update_texture` not in the seam yet).
- Vertex attributes assumed float32 tuples; no vertex pulling.
- `recover_from_device_loss` returns `Unimplemented`; real VK_ERROR_DEVICE_LOST
  handling is deferred.
- Swapchain recreation exists (`recreate_swapchain`) but is not yet wired to
  window-resize events.
- Validation layers are not enabled in the demo; add behind a debug flag later.
- `-Wmissing-field-initializers`/`-Wshadow`/`-Wsign-conversion` are
  pragma-scoped off for this TU only (Vulkan/glslang headers are C-style
  aggregate APIs); all other warnings remain enabled.

## Security/privacy review

All three dependencies pinned by commit + archive SHA-256; licences verified
and preserved in `third_party/licenses/`. Shader source is compiled by
glslang, never executed. No runtime network access.

## Migration plan

Next: uniform/camera seam (push-constant MVP or UBO), texture upload,
swapchain resize handling, then Diligent Engine evaluation (`DEC-003`).

## Related documents

- `systemplan/01`, `04`, `27`, `29`, `30`
- `DEC-001` … `DEC-004`
- `engine/renderer/docs/api.md`, `third_party/DEPENDENCIES.md`
