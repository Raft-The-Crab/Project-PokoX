# DEC-001 — Phase 0 build foundation

- **Date:** 2026-11-12
- **Status:** Accepted
- **Owner:** Raft-The-Crab
- **Category:** Engine/runtime, infrastructure

## Context

`systemplan/27` Phase 0 and `masterplan/23` Stage 0 require a reproducible
clean-checkout build before any engine subsystem is implemented. The
repository previously contained only planning documents and a 1,500-directory
scaffold. A build system, four distinct configurations, CI, tests, logging,
a dependency/licence register and design drafts were missing.

## Options considered

1. **Single-config Ninja presets with one build tree per configuration.**
   Simple, portable, works with MinGW GCC, Clang and MSVC via CMake presets.
2. **Ninja Multi-Config with one shared build tree.** Fewer directories but
   custom configuration names need extra flag/linker variables and complicate
   CI matrix isolation.
3. **Visual Studio generator only.** Matches Windows-first, but excludes the
   available MinGW toolchain and Linux CI.
4. **Vendor third-party libraries immediately.** Matches the intended stack
   but breaks the offline, reproducible, licence-clean exit gate before
   revisions and notices are verified.
5. **Adopt a third-party test framework immediately.** Convenient but adds an
   unpinned dependency at the exact point the plan asks for pinned dependencies.

## Decision

Adopt options 1 and a dependency-free core:

- `CMakePresets.json` defines `debug`, `development`, `release` and `server`,
  each with its own `build/<preset>/` tree.
- Configuration differences are expressed with generator expressions in
  `config/cmake/PokoTargets.cmake`, so both single- and multi-config generators
  work. `POKO_DEV_TOOLS` is present only in development configurations; the
  `Server` configuration defines `POKO_HEADLESS` and omits windowed targets.
- `POKO_FETCH_DEPENDENCIES` defaults to `OFF`; `poko_require_dependency()`
  blocks undeclared libraries. Registers live in
  `third_party/DEPENDENCIES.md` and `third_party/licenses/INVENTORY.md`.
- A minimal internal test harness (`tests/harness/`) keeps CTest usable offline
  until a framework is pinned.
- Implementation labels (`MOCK`, `STUB`, `LOCAL_DEV`, `PRODUCTION`) from
  `systemplan/00` are compiled into every binary via `POKO_IMPL_LABEL`.
- Files are placed inside the existing scaffold directories, so the 1,500-folder
  structure is preserved. Public headers stay flat in `engine/core/include/`
  rather than introducing new directories.

## Consequences

- Four configure/build/test command sets must be maintained; `scripts/build/
  verify_all.sh` runs them all.
- The windowed demo is a labelled `STUB` until SDL2/Vulkan are pinned; the
  headless demo runs now, satisfying the Phase 0 exit gate.
- `.clang-format` / `.clang-tidy` are configured but the lint CI job is
  advisory until the clang toolchain version is pinned.
- No third-party licence obligations are incurred in Phase 0.

## Security/privacy review

No secrets, credentials or production data are introduced. `poko_core` links
no backend or UI vendor. Logging deliberately excludes sensitive values.
The CI policy job rejects obvious committed secrets.

## Migration plan

When Phase 1 begins, pin SDL2/Vulkan revisions, copy their notices, add
`FetchContent` entries behind `POKO_FETCH_DEPENDENCIES`, and replace the
windowed demo STUB. If a test framework is adopted, replace the internal
harness and keep CTest test names stable.

## Related documents

- `systemplan/00`, `01`, `02`, `03`, `27`, `30`
- `masterplan/23`, `24`
- `docs/guides/building.md`
- `docs/architecture/core_api_and_security_design.md`
- `docs/architecture/package_manifest_draft.md`
