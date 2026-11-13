# renderer

Render device seam implementations: backend-neutral facade plus OpenGL and Vulkan backends, MOCK null device, and a PRODUCTION CPU rasterizer.

- `poko_render_software` — **PRODUCTION** `SoftwareRenderDevice`
  (`include/poko/renderer/software_device.h`). Really draws: vertex fetch from
  the bound layout, homogeneous near-plane clipping, perspective-correct
  interpolation, depth testing, face culling, triangles/strips/lines/points,
  textured and lit shading into an RGBA8 framebuffer. Needs no GPU or window,
  so servers, CI and tools produce real pixels. Shaders are not compiled: a
  fragment shader containing `poko:unlit`, `poko:vertex_color`, `poko:lambert`
  or `poko:textured` selects a built-in program. The uniform block layout
  (row-major mvp, base colour, light) is documented in the header and shared
  with `graphics::SceneRenderer`. Readback: `pixels()`, `pixel()`, `depth_at()`,
  `write_ppm()`. Not implemented: blending, mipmapped sampling, MSAA, stencil.

Public headers live under `include/poko/renderer/`; sources under `src/`;
tests under `tests/`.
