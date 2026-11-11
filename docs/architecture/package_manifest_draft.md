# Package manifest draft (Stage 0)

**Status:** DRAFT. Not a final format, not yet parsed by any runtime, and not a
confirmed decision. The owner must approve the format before implementation.

**Source:** `systemplan/13_PACKAGE_FORMATS_AND_BUILD_PIPELINE.md`,
`masterplan/26_GLOSSARY_AND_FILE_FORMATS.md`, `masterplan/14`.

## Package types

| Extension | Meaning |
|---|---|
| `.pk` | editable Poko Studio project / share file |
| `.pkx` | player game package |
| `.pkxx` | game-specific server content and configuration |

The shared Poko Dedicated Server Runtime is distributed separately; a `.pkxx`
must never bundle a duplicate runtime.

## Draft fields

The JSON Schema in `packages/schema/manifest.schema.json` captures these as a
machine-checkable draft:

- `format` and `format_version` (schema versions and migration paths).
- Immutable `game_id`, `release_id` and `package_id`.
- `runtime_compatibility`: minimum and maximum supported runtime versions.
- `mute_version`, `platforms`, `networking_modes` (`offline`,
  `player_hosted`, `dedicated`).
- `content_labels`, `permissions` / declared service capabilities.
- `dependencies` with pinned versions.
- `entry_points` for client and server.
- `limits` (sizes, counts, quotas).
- `hashes` and `signature` reference; signing keys stay server-side.
- UTC `created_utc`.

## Security constraints to enforce later

Derived from `systemplan/13` and `masterplan/26`:

- Validate magic/header and version before anything else.
- Bound archive size, file count, compression ratio, path length and recursion.
- Reject path traversal and symlinks unless explicitly supported safely.
- Verify hashes/signatures; never execute package content during validation.
- Keep signing private keys server-side; never place secrets in a package.
- Scan dependencies and preserve the exact asset version used per release.
- Only approved release IDs become publicly playable; pending updates do not
  replace the current approved release.

## Phase 0 status

`poko-packagec` and `poko-verifier` are labelled `STUB`. The schema exists to
record intent; no build, verify or inspect behaviour is implemented. Final
wording, hashes and signing design require the change process in
`masterplan/27_DECISION_LOG_AND_CHANGE_CONTROL.md`.
