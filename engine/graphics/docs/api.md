# graphics

Material/mesh -> DrawCall layer on top of the `api::IRenderDevice` seam
(systemplan/04 steps 4-5, 8). CPU-side authoring descriptors are uploaded into
device handles; a `DrawCallBuilder` assembles validated `poko::DrawCall`s ready
for `IRenderDevice::draw()`. No graphics vendor is linked; the MOCK
`NullRenderDevice` is used by the unit tests.

Layout:
- `include/poko/graphics/mesh.h` — `MeshDesc`/`GpuMesh` + `MeshLibrary`
  (uploads vertex/index buffers, generational `MeshHandle`).
- `include/poko/graphics/material.h` — `MaterialDesc`/`GpuMaterial` +
  `MaterialLibrary` (shaders, pipeline, texture/sampler bindings, uniforms).
- `include/poko/graphics/draw_call.h` — `DrawCallBuilder` + `DrawOptions`.
- `include/poko/graphics/primitives.h` — procedural cube/plane/sphere/capsule
  meshes (position + normal, CCW outward) and `make_model_matrix`.
- `include/poko/graphics/scene_renderer.h` — `SceneRenderer`: immediate-mode lit
  meshes. Takes a view-projection, a light and (mesh, model matrix, colour) per
  draw; writes a 256-byte-aligned per-draw uniform slot (mvp, colour, object
  space light) into one ring buffer and issues the draw. The caller owns
  begin_frame/clear/end_frame. Shader sources are real GLSL with `poko:` tags so
  the `SoftwareRenderDevice` can run them; GPU backends are untested here.
- `src/gfx_detail.h` — shared `detail::` helpers (byte spans, handle cleanup).
- `src/mesh.cpp`, `src/material.cpp`, `src/draw_call.cpp` — one TU per feature.
- `tests/` — module-local unit tests registered via `poko_add_unit_tests`.

The libraries own all GPU handles they create for a resource's lifetime and
release them together on `destroy()`; a failed create rolls back any handles it
already allocated. Resource creation failures and stale handles are surfaced
through `Result`/`Status` error codes at the module boundary.
