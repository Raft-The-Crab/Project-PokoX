# 00 — New Development Philosophy

## Decision
PokoX uses a **system-first, sequential subsystem completion** model. Build the machinery before polished interfaces.

## Intended order
1. Engine foundation and engine systems.
2. Networking and multiplayer.
3. MUTE and game runtime.
4. Project, asset and package build pipeline.
5. Backend and databases.
6. Dedicated server system.
7. Publishing and moderation.
8. Remaining platform services.
9. End-to-end integration.
10. Final Player, Studio and Mod interfaces.

## Definition of complete
A subsystem is not complete just because its happy path works. It needs documented responsibilities, stable interfaces, implementation for agreed scope, error handling, tests, security review, diagnostics, performance measurements, documentation and reproducible builds.

## Placeholders
Use explicit interface boundaries and label implementations:
- `MOCK`: test-only fake.
- `STUB`: interface exists, implementation absent.
- `LOCAL_DEV`: local-only, not production-safe.
- `PRODUCTION`: implemented and reviewed for release.

Mocks must never silently ship in production.

## Temporary UI and tools
Dear ImGui is appropriate for renderer controls, entity inspection, physics visualization, asset browsing, animation debugging, network statistics, MUTE debugging and server diagnostics. Keep it outside engine core and behind development build flags.

CLI tools, test clients, automated tests and headless harnesses are first-class development tools.

## UI last does not mean QA last
Test every system as it is built. Do not postpone accessibility, security, performance, usability feedback or end-to-end validation. Keep a working playable test scene throughout engine development.
