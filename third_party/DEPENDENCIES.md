# Pinned dependencies

**Status:** Phase 0 vendors **nothing**. A clean checkout builds offline.

`systemplan/02` requires dependencies to be pinned. This file is the pinning
register and must be updated *before* any library is added to the build.
`config/cmake/PokoDependencies.cmake` enforces it via
`poko_require_dependency(<name>)` and the `POKO_FETCH_DEPENDENCIES` option
(default `OFF`).

## Policy

1. Every dependency must have an **immutable** revision: a full commit SHA or a
   signed release tag. Never track a branch or `latest`.
2. Record a content hash (SHA-256 of the archive) where an archive is used.
3. Record the exact licence and preserve its notice text in
   `third_party/licenses/` (rule 11 of `systemplan/30`).
4. Verify platform support, maintenance status and licence compatibility
   before shipping (`masterplan/22`, `systemplan/29`).
5. Turn on `POKO_FETCH_DEPENDENCIES` only for a documented, pinned revision.
6. Never remove legally required notices or imply third-party code is original
   PokoX code (`masterplan/22`).

## Intended dependencies (not yet vendored)

Versions below are deliberately **not guessed**. Pin real, verified revisions
here when each subsystem starts. Inventing a version number now would create a
false record.

| Name | Intended use | Pinned revision | Hash | Licence | Status |
|---|---|---|---|---|---|
| SDL2 | windowing / input (`systemplan/04`, `07`) | TBD | TBD | zlib (verify) | NOT VENDORED |
| Vulkan headers / loader | primary renderer (`systemplan/04`) | TBD | TBD | Apache-2.0 / MIT (verify) | NOT VENDORED |
| OpenGL loader | fallback renderer (`systemplan/04`) | TBD | TBD | verify | NOT VENDORED |
| Diligent Engine | rendering integration (`systemplan/29`) | TBD | TBD | Apache-2.0 (verify) | NOT VENDORED |
| GLM | math (`masterplan/22`) | TBD | TBD | MIT (verify) | NOT VENDORED |
| miniaudio | audio backend (`systemplan/07`) | TBD | TBD | public domain / MIT (verify) | NOT VENDORED |
| Asio | networking (`masterplan/22`) | TBD | TBD | BSL-1.0 (verify) | NOT VENDORED |
| Zstandard (Zstd) | compression (`masterplan/22`) | TBD | TBD | BSD-3-Clause / GPLv2 dual (verify) | NOT VENDORED |
| glTF / importer | asset interchange (`systemplan/05`) | TBD | TBD | verify per importer | NOT VENDORED |
| Dear ImGui | development/debug UI only (`systemplan/00`) | TBD | TBD | MIT (verify) | NOT VENDORED |
| egui | Windows UI (Rust, `systemplan/25`) | TBD | TBD | MIT / Apache-2.0 (verify) | NOT VENDORED |
| Kotlin / Android SDK | Android UI (`systemplan/25`) | TBD | TBD | verify SDK terms | NOT VENDORED |
| Catch2 / doctest | unit tests (future) | TBD | TBD | BSL-1.0 / MIT (verify) | NOT VENDORED |

**"verify"** means the licence has not been checked against the actual pinned
release. Free-tier limits, product terms and availability can change; recheck
them before implementation (`masterplan/README.md`).

## Current build

`poko_core`, the CLI tools, the demos and the test harness use only the C++20
standard library. `tests/harness/poko_test_harness.h` avoids a third-party test
framework until one is pinned above.
