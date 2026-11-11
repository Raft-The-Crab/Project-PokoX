# 03 — Engine Core

## Scope
Base types, IDs/handles, math, memory ownership, logging, diagnostics, event system, job system, clocks, configuration, platform abstraction and resource lifetime.

## Rules
- Explicit ownership and thread-safety contracts.
- Avoid global mutable state.
- Keep core independent of graphics, backend and UI.
- Define stable serialized IDs.
- Standardize errors and module boundaries.

## Demo
Build console and windowed test applications. Add ImGui only as a development layer.

## Exit gate
Core APIs documented; tests cover math, IDs, events, jobs, timing and resource lifetime; no backend or final UI dependency.
