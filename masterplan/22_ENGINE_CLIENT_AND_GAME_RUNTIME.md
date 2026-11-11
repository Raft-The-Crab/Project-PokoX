# 22 — Engine, Client and Game Runtime

## Engine direction
- C++20 core.
- Vulkan primary renderer with OpenGL fallback as planned.
- SDL2 input/windowing.
- GLM math.
- miniaudio.
- Asio networking.
- Zstandard compression.
- glTF and other asset loading.
- Custom physics system, with independent testing and documented limitations.
- Diligent Engine shader/rendering integration as selected.

Verify library licenses, platform support and maintenance before shipping. Abstract dependencies behind Poko APIs for architecture consistency, but do not remove legally required notices or imply third-party code is original Poko code.

## Shared core
Shared modules:
- Math and utilities.
- Asset/resource system.
- Entity/component model.
- Physics interfaces.
- MUTE runtime.
- Serialization.
- Networking abstractions.
- Diagnostics.

Client-only:
- Rendering.
- Audio output.
- Input.
- UI.
- Camera.
- Animation presentation.
- Prediction/interpolation.

Server-only:
- Session and authority.
- State validation.
- Game rules.
- Server simulation.
- Persistence interface.
- Network replication.
- Anti-abuse checks.

## Authority
Clients can predict presentation for responsiveness, but server validates important state: movement bounds, inventory, damage, checkpoints, trades, purchases and reward claims. Offline games can use local authority, with clear limits for cloud-connected features.

## Character system
- R6 default.
- Custom accessory attachment system.
- Define stable named attachment points.
- Rig-independent animation and equipment interfaces.
- Add R15 and other rigs later without changing every game API.

## First official games
- Block-building prototype.
- Obby prototype.

Use these as engine testbeds for terrain/blocks, physics, save/load, multiplayer, avatars, UI, server authority and performance.
