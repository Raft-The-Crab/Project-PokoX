# 27 — Development Phases and Exit Criteria

## Phase 0: Build foundation
Repository, CI, dependencies, tests and logging. Exit: clean reproducible build.

## Phase 1: Complete engine
Core, renderer, assets, physics, audio, input, animation, character, serialization and debug tools. Exit: stable local demo scene, documented APIs, headless/windowed harnesses and performance baseline.

## Phase 2: Networking
Transport, protocol, replication, prediction, authority and modes. Exit: multiplayer sample across processes.

## Phase 3: MUTE/runtime
Interpreter, standard library, sandbox, debugger and service APIs. Exit: bounded sample game runs.

## Phase 4: Packages
`.pk`, `.pkx`, `.pkxx`, build, signing and verification. Exit: CLI build/verify/launch.

## Phase 5: Backend
Identity, accounts, catalog, storage, permissions, server registry, APIs and audit. Exit: API workflows tested with security controls.

## Phase 6: Dedicated hosting
Runtime, registration, tickets, heartbeat, diagnostics and docs. Exit: external test developer can host sample game.

## Phase 7: Publishing/moderation
Forms, validation, Review Runtime, Inspector, human review, four attempts, appeal and reports. Exit: complete submit/review/retry/approve/publish flow.

## Phase 8: Remaining platform
Groups, teams, social, economy, marketplace, notifications, analytics, support and operations. Exit: launch-scope APIs and tests complete.

## Phase 9: Integration
CLI/test clients/ImGui. Exit: critical journeys work without final UI.

## Phase 10: Final UI
Player, Studio and Mod; Windows first, Android when ready. Exit: workflows, accessibility, performance and release QA.

Parallelize documentation, test design and interface prototypes early. Full implementation focus follows dependency order.
