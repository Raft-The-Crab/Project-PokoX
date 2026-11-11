# Build profiles

The four configurations required by `systemplan/02` are defined in
`config/cmake/PokoConfigurations.cmake` and exposed through
`CMakePresets.json`. Use a preset rather than passing `-DCMAKE_BUILD_TYPE`
manually.

| Preset | `CMAKE_BUILD_TYPE` | Asserts | Dev tools | Headless | Optimisation |
|---|---|---|---|---|---|
| `debug` | `Debug` | on | on | no | none |
| `development` | `Development` | on | on | no | `-O0 -g` + profiling |
| `release` | `Release` | off | off | no | `-O3 -DNDEBUG` |
| `server` | `Server` | off | off | **yes** | `-O2 -DNDEBUG` |

`Server` is headless: no graphics, audio or player UI targets are built, and
`POKO_HEADLESS` is defined. `POKO_DEV_TOOLS` exists only in `Debug` and
`Development`, so Dear ImGui and other debug UI cannot leak into `Release` or
`Server` (systemplan/00, systemplan/30 rule 12).

Compile definitions are applied with generator expressions in
`config/cmake/PokoTargets.cmake`, so they work with both single-configuration
(Ninja) and multi-configuration generators.

## Usage

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

See `docs/guides/building.md` for clean-checkout instructions and the exit
gate. Each profile directory here is a placeholder for future per-profile
toolchain and environment files.
