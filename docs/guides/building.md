# Building PokoX (Phase 0)

This is the documented clean-checkout build path required by the Stage 0 exit
gate (`systemplan/02_REPOSITORY_AND_BUILD_FOUNDATION.md`, `systemplan/27`,
`masterplan/23`). A clean checkout must build reproducibly, CI must run tests,
no secrets may exist in history, development and release flags must be
distinct, and minimal windowed and headless demos must run.

## Prerequisites

| Tool | Minimum | Notes |
|---|---|---|
| CMake | 3.25 | `CMakePresets.json` version 6 |
| Ninja | 1.x | generator used by all presets |
| C++20 compiler | GCC 12+, Clang 15+ or MSVC 19.3+ | MinGW-w64 GCC 16.2 verified on Windows |

No third-party library is required. Phase 0 vendors nothing and never contacts
the network (`POKO_FETCH_DEPENDENCIES=OFF`). See `third_party/DEPENDENCIES.md`.

### Windows with MinGW (verified here)

`cl.exe` is not on PATH in a plain shell, so tell CMake which compiler to use
on the first configure of each build tree:

```sh
export CXX=g++
export CC=gcc
cmake --preset debug
```

### Windows with MSVC

Run from a "Developer Command Prompt for VS" (or the equivalent environment
setup) so `cl.exe` is on PATH, then use the presets normally.

### Linux

```sh
cmake --preset debug
```

## Configure, build and test

Four presets correspond to the four required configurations:

```sh
cmake --preset debug
cmake --build --preset debug --parallel
ctest --preset debug --output-on-failure
```

Replace `debug` with `development`, `release` or `server`. Each preset uses its
own build tree at `build/<preset>/`, so switching configurations never mixes
object files. `profiles/README.md` documents what each configuration enables.

To verify all four in one pass:

```sh
bash scripts/build/verify_all.sh
```

## Targets

| Target | Label | Purpose |
|---|---|---|
| `poko_core` | `PRODUCTION` | version, build config, IDs, logging, assertions |
| `poko_api_seams` | interface | STUB declarations of the early interface seams |
| `poko_headless` | `LOCAL_DEV` | runnable headless demo (exit gate) |
| `poko_windowed_demo` | `STUB` | windowed demo placeholder until SDL2/Vulkan are pinned |
| `poko_assetc` | `STUB` | asset compiler CLI |
| `poko_packagec` | `STUB` | `.pk`/`.pkx`/`.pkxx` package builder |
| `poko_verifier` | `STUB` | package signature/manifest verifier |
| `poko_serverctl` | `STUB` | dedicated server diagnostics CLI |
| `poko_server` | `STUB` | shared headless dedicated server runtime |
| `poko_test_*` | — | four unit-test executables registered with CTest |

`STUB`, `LOCAL_DEV`, `MOCK` and `PRODUCTION` are the implementation labels from
`systemplan/00`. Labels are compiled into each binary (`POKO_IMPL_LABEL`) and
can be seen with `--version`, so a stub cannot be mistaken for production.
`MOCK` is unused in Phase 0.

The `Server` configuration is headless: `poko_windowed_demo` is not built and
`POKO_HEADLESS` is defined. `POKO_DEV_TOOLS` exists only in `Debug` and
`Development`, keeping debug UI out of release builds.

## Running the demos

```sh
./build/debug/runtime/game/poko_headless
./build/debug/examples/hello_world/poko_windowed_demo
./build/debug/tools/poko_assetc --version
```

Both demos are also CTest tests (`runtime.headless` and `demo.windowed`), so a
passing `ctest` run proves they execute.

## Exit-gate checklist

- [ ] Clean checkout configures with `cmake --preset <preset>`.
- [ ] All four presets build without errors.
- [ ] `ctest --preset <preset>` passes for every configuration.
- [ ] `build/debug/compile_commands.json` and `build/release/compile_commands.json`
      differ in `POKO_ENABLE_ASSERTS` / `POKO_DEV_TOOLS` / `POKO_RELEASE_BUILD`.
- [ ] No secrets in tracked files (`CI` policy job checks this).
- [ ] Both demos run.

## CI

`.github/workflows/ci.yml` builds and tests all four configurations on Windows
and Ubuntu on a clean checkout. A separate advisory job runs `clang-format` and
`clang-tidy` (config in `.clang-format` / `.clang-tidy`); it becomes blocking
once the clang toolchain version is pinned. Workflows run only on pushes/PRs,
which require explicit owner authorisation per `CODEBUDDY.md`.
