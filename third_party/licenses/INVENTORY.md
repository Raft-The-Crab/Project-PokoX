# Third-party licence inventory

**Revision:** 2026-11-12
**Status:** Phase 0 — no third-party code is vendored. All rows are planned.

`systemplan/02` requires a licence inventory; rule 11 of `systemplan/30`
requires preserving third-party licences and notices. The software bill of
materials is tracked in `security/sbom/` once dependencies are pinned.

| Component | Planned licence | Notice text present? | Verification | Status |
|---|---|---|---|---|
| SDL2 | zlib (unverified) | no | required | NOT VENDORED |
| Vulkan headers / loader | Apache-2.0 / MIT (unverified) | no | required | NOT VENDORED |
| OpenGL loader | unverified | no | required | NOT VENDORED |
| Diligent Engine | Apache-2.0 (unverified) | no | required | NOT VENDORED |
| GLM | MIT (unverified) | no | required | NOT VENDORED |
| miniaudio | public domain / MIT (unverified) | no | required | NOT VENDORED |
| Asio | BSL-1.0 (unverified) | no | required | NOT VENDORED |
| Zstandard | BSD-3-Clause / GPLv2 dual (unverified) | no | required | NOT VENDORED |
| glTF importers | per-importer, unverified | no | required | NOT VENDORED |
| Dear ImGui | MIT (unverified) | no | required | NOT VENDORED |
| egui | MIT / Apache-2.0 (unverified) | no | required | NOT VENDORED |
| Catch2 / doctest | BSL-1.0 / MIT (unverified) | no | required | NOT VENDORED |

## Rules before any dependency is added

1. Pin the exact revision in `third_party/DEPENDENCIES.md` first.
2. Copy the full licence text and required notices into this directory, named
   after the component (for example `SDL2-LICENSE.txt`).
3. Record the component in the SBOM under `security/sbom/`.
4. Confirm redistribution rights under the proprietary `LICENSE` at the repo
   root, and check for copyleft obligations.
5. Verify platform support and maintenance status (`systemplan/29`).

A public release must not proceed with an unresolved licence row
(`masterplan/24`, `masterplan/25` risk register).
