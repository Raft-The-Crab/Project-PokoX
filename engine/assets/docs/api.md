# assets

Async asset loading seam (`api::IAssetProvider`). Two implementations, separate
targets so a MOCK is never confused with production (systemplan/30 rule 12):

- `poko_assets_null` — **MOCK** `NullAssetProvider`
  (`include/poko/assets/provider.h`). Synthesizes deterministic payloads, no
  I/O. Used by the runtime demos/tests.
- `poko_assets_fs` — **PRODUCTION** `FilesystemAssetProvider`
  (`include/poko/assets/filesystem_provider.h`). Performs real reads through an
  injected `api::IPlatformFileSystem`, decodes the `PKX1` container and caches
  decoded payloads by logical path.

`PKX1` (documented in `src/fs_detail.h`) is a 20-byte little-endian header
(`magic "PKX1" | kind | a | b | c`) followed by an opaque payload. Structured
fields are decoded for Mesh (vertex/index/submesh counts), Texture
(width/height/format) and Audio (sample rate/channels/bits + duration from the
payload size); other kinds stay opaque. Real importers (glTF, PNG, WAV) replace
this decoder later without changing the seam.

`request_load()` queues, `update()` performs the read + decode (cache hits skip
I/O entirely), `poll_load()` is one-shot, and `release_asset()` uses
generation-checked handles. `cache_size()`, `failed_load_count()` and `evict()`
are backend-specific introspection. Paths remain package-relative — the
filesystem seam rejects rooted/traversal paths.

Public headers live under `include/poko/assets/`; sources under `src/`;
tests under `tests/`.
