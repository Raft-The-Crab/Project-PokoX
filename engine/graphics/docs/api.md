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
- `src/gfx_detail.h` — shared `detail::` helpers (byte spans, handle cleanup).
- `src/mesh.cpp`, `src/material.cpp`, `src/draw_call.cpp` — one TU per feature.
- `tests/` — module-local unit tests registered via `poko_add_unit_tests`.

The libraries own all GPU handles they create for a resource's lifetime and
release them together on `destroy()`; a failed create rolls back any handles it
already allocated. Resource creation failures and stale handles are surfaced
through `Result`/`Status` error codes at the module boundary.
