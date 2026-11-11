# DEC-004 — SDL3 windowing and the OpenGL render backend

- **Date:** 2026-11-12
- **Status:** Accepted
- **Owner:** Raft-The-Crab
- **Category:** Engine/runtime

## Context

`DEC-003` deferred real graphics backends until dependencies could be pinned.
Two findings shaped this decision:

1. **SDL2 is end-of-life.** Upstream states there are no further planned SDL
   2.0 releases and directs users to SDL3 (or `sdl2-compat`). Pinning SDL2
   2.32.10 would have frozen us on an unmaintained library.
2. **gl3w cannot be fetched as-is.** Its CMake build runs `gl3w_gen.py` at
   configure time, which downloads Khronos headers from `registry.khronos.org`
   — an unpinned, mutable fetch that breaks offline reproducibility.

## Options considered

1. **Pin SDL2 2.32.10 + a different GL loader (GLAD/GL3W FetchContent).**
   SDL2 is EOL; gl3w's build-time download violates the pinning policy.
2. **Pin SDL3 + vendor generated gl3w sources.** SDL3 is the maintained
   upstream with the best long-term platform coverage (Windows, Linux,
   Android). Vendoring gl3w's generated output removes its network fetch.
3. **Pin SDL3 + hand-rolled GL loader via `SDL_GL_GetProcAddress`.** Avoids
   vendoring but reinvents a loader and covers fewer functions.

## Decision

Adopt option 2.

- **SDL3** `release-3.4.20` (commit `f00dc919ca35b9756316e86c4777392de442f9b2`)
  is fetched via `FetchContent` as a SHA-256-verified codeload tarball, only
  when `POKO_FETCH_DEPENDENCIES=ON`. Built static (`SDL3::SDL3`), licence zlib
  (`third_party/licenses/SDL3-LICENSE.txt`).
- **gl3w** commit `372e391702acac616e0fd32e3eaccf19d58b9626` (The Unlicense) is
  vendored as *generated* sources under `third_party/vendored/gl3w/` —
  `gl3w.c`, `gl3w.h`, `glcorearb.h`, `khrplatform.h` — with provenance and
  input hashes in `VENDORED.md`. No build-time network access.
- New option `POKO_RENDER_OPENGL` (default `OFF`) gates the whole path and
  hard-fails without `POKO_FETCH_DEPENDENCIES=ON`. Default builds stay fully
  offline.
- `poko_render_gl` [PRODUCTION] implements `api::IRenderDevice` for OpenGL
  3.3 core: buffers (with the new `update_buffer` seam), textures (2D,
  non-array), GLSL shader compile/link, pipeline state (cull/depth/topology),
  clear/draw/frame lifecycle. `poko_windowed_demo` becomes a real SDL3 window
  rendering a triangle (`--frames N` for automated runs); the STUB remains for
  offline builds.
- `IRenderDevice` gains `update_buffer(handle, span<byte>, offset)` — required
  by every real backend; the MOCK records and bounds-checks it.
- clang-format 23.1.5 and clang-tidy 23.1.4 are pinned (PyPI wheels) and the
  CI lint job is now blocking.

## Consequences

- A real window and triangle render on GL 3.3 hardware (verified locally on
  AMD Radeon, 60-frame smoke run).
- Deferred, recorded honestly: Vulkan remains the stated primary backend
  (headers/loader/Diligent still `NOT VENDORED`); GL context-loss detection is
  not portable on WGL/GLX so `device_lost()` reports not-lost; no uniform/UBO
  or texture-upload seam yet (needed for camera/lighting, steps 6-7); vertex
  attributes are assumed `float32` tuples; `max_buffer_bytes` is a conservative
  1 GiB floor (GL has no direct query).
- SDL2 references in planning docs are superseded by SDL3; `sdl2-compat`
  remains an option if an SDL2 API surface is ever required.

## Security/privacy review

SDL3 and gl3w are pinned by commit SHA and archive SHA-256; licences verified
and preserved. Shader sources are compiled, never executed as code. No
secrets, network calls at runtime, or untrusted package execution.

## Migration plan

Next renderer increments in `systemplan/04` order: uniforms/camera path
(seam extension), texture upload, then pin Vulkan headers/loader and implement
`VulkanRenderDevice` as the primary backend.

## Related documents

- `systemplan/01`, `04`, `27`, `29`, `30`
- `DEC-001`, `DEC-002`, `DEC-003`
- `engine/renderer/docs/api.md`
- `third_party/DEPENDENCIES.md`, `third_party/vendored/gl3w/VENDORED.md`
