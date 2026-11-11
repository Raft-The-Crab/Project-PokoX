# DEC-002 — Phase 1 engine core

- **Date:** 2026-11-12
- **Status:** Accepted
- **Owner:** Raft-The-Crab
- **Category:** Engine/runtime

## Context

Phase 0 delivered a buildable foundation (`DEC-001`). `systemplan/27` Phase 1
starts with "Core, renderer, assets, physics, audio, input, animation,
character, serialization and debug tools". Following the system-first,
sequential-subsystem model (`systemplan/00`), the first subsystem to complete is
**engine core** (`systemplan/03`): base types, IDs/handles, math, memory
ownership, logging, diagnostics, event system, job system, clocks,
configuration, platform abstraction and resource lifetime.

## Options considered

1. **Vendor GLM / a task-parallelism library immediately.** Faster to write,
   but unpinned revisions and unverified licences break `DEC-001`'s offline,
   licence-clean policy.
2. **Implement a small dependency-free core now, replace selectively later.**
   Keeps the build reproducible and gives full control over coordinate system,
   units, determinism and thread-safety contracts.
3. **Jump straight to the renderer.** Violates the dependency order in
   `systemplan/01` (renderer depends on engine core).

## Decision

Adopt option 2. Add to `engine/core`:

- `poko_result.h` — standardized `ErrorCode`, `Status`, `Result<T>`.
- `poko_math.h` — right-handed, +Y up, -Z forward; metres/radians/seconds;
  row-major matrices; documented determinism and epsilon comparisons.
- `poko_clock.h` — steady-clock `Stopwatch` and capped `FixedTimestep`.
- `poko_event.h` — instance-based `EventBus<E>` with subscription handles;
  handlers dispatch outside the lock.
- `poko_job.h/.cpp` — `JobSystem` implementing `api::IJobSystem`; owns its
  worker threads, contains job exceptions.
- `poko_diagnostics.h`, `poko_settings.h`, `poko_ownership.h` — instance-based
  diagnostics, typed configuration and explicit ownership/lifetime helpers.
- `engine/platform_abstraction/` — `LocalFileSystem` implementing
  `api::IPlatformFileSystem`, root-scoped with traversal rejection.

`IPlatformFileSystem` and `IJobSystem` receive real method signatures; the
other seams remain `STUB` declarations. `poko_platform` is labelled
`LOCAL_DEV`, `poko_core` remains `PRODUCTION`.

The console `poko_headless` demo exercises all of these subsystems, satisfying
the `systemplan/03` demo requirement. The windowed demo remains a labelled
`STUB` until SDL2/Vulkan are pinned.

## Consequences

- A small custom math library must be maintained until GLM is pinned and
  verified. Coordinate-system and determinism assumptions are recorded in
  `engine/core/docs/api.md` so later physics work (`systemplan/06`) can rely on
  them.
- The job system uses `std::thread`; platform-specific thread affinity,
  priorities and fibre-based scheduling are deferred.
- `LocalFileSystem` is local-development only; production package I/O must add
  signature verification and isolation (`systemplan/13`).
- All new headers are flat in `engine/core/include/`, so the 1,500-folder
  scaffold is unchanged.

## Security/privacy review

No secrets or network access. `LocalFileSystem` rejects rooted paths and `..`
components so untrusted paths cannot escape its root. Job exceptions are
contained rather than terminating the process. Logging still excludes
sensitive values.

## Migration plan

When GLM, a task library or a real asset filesystem are pinned, replace the
corresponding core pieces behind the same headers and keep the documented
coordinate/units contract. Renderer work (`systemplan/04`) can then begin.

## Related documents

- `systemplan/00`, `01`, `03`, `06`, `09`, `27`, `30`
- `DEC-001` (`docs/decisions/0001-phase0-build-foundation.md`)
- `engine/core/docs/api.md`
- `docs/architecture/core_api_and_security_design.md`
