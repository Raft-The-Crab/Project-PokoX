# Pinned dependencies

**Status:** SDL3 is pinned for opt-in fetching; generated gl3w sources are
vendored. With `POKO_FETCH_DEPENDENCIES=OFF` (the default) a clean checkout
still builds fully offline.

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
| SDL3 | windowing / input (`systemplan/04`, `07`) — replaces SDL2, which is EOL upstream (DEC-004) | `f00dc919ca35b9756316e86c4777392de442f9b2` (tag `release-3.4.20`) | SHA-256 `e6a9c4e75de8fe719f922dddc208627f77754a502909f457a1355981f20c21e0` (codeload tarball) | zlib — verified, `licenses/SDL3-LICENSE.txt` | PINNED (FetchContent, `POKO_RENDER_OPENGL`) |
| gl3w | OpenGL core profile loader (`systemplan/04`) — generated sources vendored, see `vendored/gl3w/VENDORED.md` | `372e391702acac616e0fd32e3eaccf19d58b9626` | SHA-256 `2d9f0aa9716c3bbc262965ace6caaa1b901cdc37616dafad590cffb35bfc419d` (codeload tarball) | The Unlicense (public domain) — verified, `licenses/gl3w-UNLICENSE.txt` | VENDORED |
| Vulkan headers / loader | primary renderer (`systemplan/04`) | TBD | TBD | Apache-2.0 / MIT (verify) | NOT VENDORED |
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

## Development tools (CI only, not shipped)

| Name | Pinned version | Licence | Used by |
|---|---|---|---|
| clang-format | 23.1.5 (PyPI wheel `clang_format-23.1.5-py2.py3-none-win_amd64.whl`, SHA-256 `18a4a1a698dd52ed4e288c97d99e93910f8f9d0128b266f65021ecf39c85d034`) | Apache-2.0 WITH LLVM-exception | CI lint job, `.clang-format` |
| clang-tidy | 23.1.4 (PyPI wheel) | Apache-2.0 WITH LLVM-exception | CI lint job |

## Current build

`poko_core`, `poko_platform`, `poko_renderer`, `poko_render_null`, the CLI
tools, the demos and the test harness use only the C++20 standard library.
`tests/harness/poko_test_harness.h` avoids a third-party test framework until
one is pinned above.

**Renderer note (`DEC-003`, `DEC-004`):** `systemplan/04` ships a labelled MOCK
`NullRenderDevice` by default. Setting `POKO_RENDER_OPENGL=ON` (which requires
`POKO_FETCH_DEPENDENCIES=ON`) fetches the pinned SDL3 tarball, builds the
`poko_render_gl` OpenGL device from the vendored gl3w loader, and turns
`poko_windowed_demo` into a real clear-screen-and-triangle window. Vulkan
headers/loader and Diligent Engine remain `NOT VENDORED`; until they are pinned
no Vulkan or Diligent target may be added.
