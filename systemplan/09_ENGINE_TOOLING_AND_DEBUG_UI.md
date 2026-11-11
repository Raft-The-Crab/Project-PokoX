# 09 — Engine Tooling and Debug UI

Dear ImGui development windows:
- Engine/frame timing.
- Renderer/GPU capabilities.
- Scene hierarchy and entity inspector.
- Asset browser.
- Physics visualization.
- Animation preview.
- Audio controls.
- Network inspector.
- MUTE console/debugger.
- Memory/job statistics.
- Logs and crash viewer.

Keep debug UI outside engine core, development-only, secret-free and excluded from release builds. Add CLI asset compiler, package verifier, test-scene runner and benchmark runner.

Exit gate: developers can inspect engine systems without final Player or Studio UI.
