# Vendored gl3w (OpenGL core profile loader)

This directory contains the **generated** gl3w sources, vendored so that a
clean checkout builds fully offline. Upstream gl3w's own CMake build runs
`gl3w_gen.py` at configure time, which downloads Khronos headers from
`registry.khronos.org` — an unpinned, mutable fetch. Vendoring the generated
output removes that network dependency.

## Provenance

- Upstream: https://github.com/skaslev/gl3w
- Pinned commit: `372e391702acac616e0fd32e3eaccf19d58b9626`
  (2026-08-17, "Handle the possibility of GetProcAddress(libgl,
  \"wglGetProcAddress\") failing")
- Source archive SHA-256:
  `2d9f0aa9716c3bbc262965ace6caaa1b901cdc37616dafad590cffb35bfc419d`
  (`https://codeload.github.com/skaslev/gl3w/tar.gz/372e391702acac616e0fd32e3eaccf19d58b9626`)
- Licence: The Unlicense (public domain) — see `UNLICENSE` in this directory
  and `third_party/licenses/gl3w-UNLICENSE.txt`.

## Generated inputs (frozen 2026-11-12)

`gl3w_gen.py` fetched these Khronos registry headers when this copy was
generated. They are committed here verbatim so regeneration is never required:

| File | Source URL | SHA-256 |
|---|---|---|
| `include/GL/glcorearb.h` | `https://registry.khronos.org/OpenGL/api/GL/glcorearb.h` | `65fae555a8b3b5709099837001e0c14228b3e45651e2d0fa2ad22649833921e1` |
| `include/KHR/khrplatform.h` | `https://registry.khronos.org/EGL/api/KHR/khrplatform.h` | `7b1e01aaa7ad8f6fc34b5c7bdf79ebf5189bb09e2c4d2e79fc5d350623d11e83` |

Generated outputs (also committed):

| File | SHA-256 |
|---|---|
| `include/GL/gl3w.h` | `ede493c73296bbb6642f3f7614ccd4f2f4fc8b186efce9d938aef150b8227efd` |
| `src/gl3w.c` | `b34e06808e060bc2aec3d105e2fa04cc7978b31f89889f4affcc1e591d097f96` |

To regenerate intentionally: run `python3 gl3w_gen.py` from upstream at the
pinned commit, then re-vendor and update the hashes above.
