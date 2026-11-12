# Workbuddy Rules — PokoX

## File & folder organization (REQUIRED for all new code)

Every `engine/<module>/` must follow the full module layout — no exceptions:

```
engine/<module>/
  CMakeLists.txt
  include/poko/<module>/<name>.h   # public headers, namespaced include path
  src/<name>.cpp                   # implementation, split per class/feature
  tests/<name>_test.cpp            # module-local unit tests
  docs/api.md                      # module purpose + API notes
```

Rules:

1. **Namespaced includes.** Public headers live under `include/poko/<module>/` and are included as `#include "poko/<module>/<name>.h"` — never flat `poko_*.h` names.
2. **Split sources per class/feature.** No monolithic single-file modules. One `.cpp` per class, or per feature area for large classes (e.g. `physics` → `bodies.cpp`/`raycast.cpp`/`step.cpp`; `scene` → `nodes.cpp`/`transforms.cpp`). Member functions may be defined across multiple TUs. Shared file-local helpers go in a `src/` internal header (e.g. `gl_conversions.h`, `vk_conversions.h`, `utf8.h`) as `inline` functions under a detail namespace — never duplicated.
3. **Tests live in the module.** Unit tests go in `engine/<module>/tests/` and are registered via `poko_add_unit_tests(<target> TEST_SOURCES ...)` in the module's `CMakeLists.txt`. Do NOT recreate a central `tests/unit/` directory.
4. **Docs per module.** Each module keeps `docs/api.md` describing purpose and layout.
5. **CMake.** Each module is its own `add_library` target with `target_include_directories(... include)` PUBLIC, links `poko_core`, and uses `poko_apply_common` + `poko_set_impl_label` (MOCK/STUB/LOCAL_DEV/PRODUCTION). Update the module `CMakeLists.txt` source list whenever adding/removing `.cpp` files.
6. **No `.gitkeep` files.** Empty dirs are not tracked; don't reintroduce placeholders.
7. **Never run clang-format on `CMakeLists.txt`** — it mangles CMake syntax. Format only `.h`/`.cpp` files.

## Git / GitHub rule

- Commit locally as work completes; **do not push unless the user explicitly says "push"**.
- Commit message style: concise imperative subject focused on *why*, body optional, then the trailer:

```
Generated with [Devin](https://devin.ai)

Co-Authored-By: Devin <158243242+devin-ai-integration[bot]@users.noreply.github.com>
```

- Verify before committing: configure + build the relevant preset(s), run `ctest`, confirm no stale includes or unintended diff.
- Repo: `https://github.com/Raft-The-Crab/Project-PokoX.git`, branch `main`.
