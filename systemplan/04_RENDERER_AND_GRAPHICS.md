# 04 — Renderer and Graphics

Planned direction: Vulkan primary, OpenGL fallback where practical, Diligent Engine integration as selected.

## Build sequence
1. Window/surface and device.
2. Clear screen and triangle.
3. Shader compilation/cache and pipelines.
4. Vertex/index buffers and meshes.
5. Textures and materials.
6. Camera/transforms.
7. Depth and basic lighting.
8. Scene submission/batching.
9. GPU timing and diagnostics.
10. Fallback, device-loss and resource failure handling.

## Debug demo
ImGui GPU information, frame time, draw calls, shader reload, wireframe and material inspection.

## Exit gate
A deterministic sample scene renders on agreed minimum hardware, fallback behavior is tested, resource failures are handled and performance is measured.
