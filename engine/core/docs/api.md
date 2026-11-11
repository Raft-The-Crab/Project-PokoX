# Engine core API reference (Phase 1)

**Status:** implemented and unit-tested. This is the "Core APIs documented"
deliverable of `systemplan/03`. Engine core links no graphics, backend, UI or
third-party library.

## Module map

| Header | Responsibility |
|---|---|
| `poko_version.h` | version constants, config and platform names |
| `poko_build_config.h` | compile-time configuration flags |
| `poko_types.h` | stable IDs, handles, timestamps |
| `poko_result.h` | `ErrorCode`, `Status`, `Result<T>` |
| `poko_math.h` | vectors, matrices, quaternions, constants |
| `poko_clock.h` | `Stopwatch`, `FixedTimestep` |
| `poko_event.h` | `EventBus<Event>` |
| `poko_job.h` | `JobSystem` (implements `api::IJobSystem`) |
| `poko_diagnostics.h` | counters, gauges, `ScopedTimer` |
| `poko_settings.h` | typed configuration store |
| `poko_ownership.h` | `ObserverPtr`, `ScopeExit`, `ResourceRegistry` |
| `poko_log.h` | structured logging and `POKO_LOG_*` macros |
| `poko_assert.h` | `POKO_ASSERT`, `AssertionError` |
| `poko_api_seams.h` | interface seams from `systemplan/01` |

`poko_platform_fs.h` (`engine/platform_abstraction/`) implements
`api::IPlatformFileSystem`.

## Conventions

### Coordinate system and units

Right-handed, **+Y up, -Z forward**. Metres, radians, seconds. Matrices are
**row-major** and act on column vectors (`M * v`). `transform_point` uses w=1
(translation applies); `transform_direction` uses w=0. IEEE-754 single
precision, no fast-math. Use `nearly_equal(a, b, epsilon)` for comparisons.
These match the documented physics targets in `systemplan/06`.

### IDs and handles

`EntityId` is a stable 64-bit serialized ID; `0` is invalid. Display names are
never IDs. `TypedHandle` combines an index with a generation; a released slot
bumps its generation so stale handles cannot alias a new resource.

### Errors

`Status` for void operations, `Result<T>` for values. Always check `ok()` /
`has_value()` before `value()` (which throws `std::bad_variant_access` on
misuse). Error codes are stable; `to_string(ErrorCode)` returns their names.

### Ownership

- `std::unique_ptr` = owning; `ObserverPtr<T>` = non-owning borrow.
- `ResourceRegistry<T>` owns resources and hands out generation-checked
  handles. Borrowed pointers are valid only while the handle is live.
- `ScopeExit` runs deterministic cleanup unless `release()` is called.

### Thread safety

| Type | Contract |
|---|---|
| `Logger` | `set_level`/`is_enabled` are atomic; sink changes and `log` are mutex-protected |
| `EventBus<E>` | all methods concurrent-safe; handlers run outside the lock and may publish/subscribe |
| `JobSystem` | `submit`/`wait_idle` from any thread; jobs may run concurrently; exceptions are logged and contained |
| `Diagnostics` | all methods concurrent-safe; timings are last-value-wins |
| `Settings` | all methods concurrent-safe |
| `ResourceRegistry<T>` | all methods concurrent-safe; borrowed pointers need external synchronisation |
| `LocalFileSystem` | const methods are concurrent-safe |

No mutable global state except the `Logger` singleton, which is deliberately
process-wide for diagnostics. Everything else is instance-based.

### Logging

Never log credentials, tokens, personal data or sensitive moderation evidence
(`masterplan/17`, `systemplan/23`). `Logger::set_sink()` allows tests and a
future crash-reporting layer to capture records. `POKO_ASSERT` exists only in
`Debug` and `Development`; it is a no-op in `Release` and `Server`.

## Interface seams

`IPlatformFileSystem` and `IJobSystem` now have concrete signatures and are
implemented by `LocalFileSystem` (LOCAL_DEV) and `JobSystem`. The remaining
seams (`IRenderDevice`, `IAssetProvider`, `IPhysicsWorld`, `IAudioDevice`,
`INetworkTransport`, `IScriptRuntime`, `IPlatformServices`, `IAccountProvider`,
`IGameCatalog`, `IPackageStore`, `IServerRegistry`) stay as `STUB` declarations
until their owning subsystems are built.

## Build configurations

`Debug`: asserts + dev tools. `Development`: asserts + dev tools + profiling.
`Release`: optimised, no asserts, no dev tools. `Server`: headless, no
graphics/audio/UI. See `profiles/README.md`.

## Tests

`tests/unit/` covers math, IDs, result/status, logging, assertions, clocks,
events, jobs, diagnostics, settings, ownership/resource lifetime and the
platform filesystem. Run with `ctest --preset <preset>`.
