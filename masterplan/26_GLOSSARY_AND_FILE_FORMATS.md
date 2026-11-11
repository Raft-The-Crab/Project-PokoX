# 26 — Glossary and File Formats

## Product terms
- **PokoX Player:** player-facing game client.
- **Poko Studio:** creator/editor application.
- **Poko Mod:** moderation and review application.
- **Review Runtime:** isolated execution environment for submitted games.
- **Game Inspector:** tools for examining game packages and runtime behavior.
- **MUTE:** Poko's interpreted scripting language.
- **Pokoins:** PokoX virtual currency.
- **Poko Dedicated Server Runtime:** shared headless runtime for developer-hosted game servers.

## File formats
### `.pkx`
Player game package. Should include a manifest, game assets, scripts and dependency metadata as appropriate. It must be signed/hashed and safely extracted.

### `.pk`
Poko Studio project/share file. Define whether it contains editable source, asset references, team metadata and local-only data. Never include credentials.

### `.pkxx`
Game-specific server package. Contains server-side game content/configuration and compatibility metadata. Shared runtime is distributed separately.

## Package security
- Validate magic/header and version.
- Limit archive size, file count, compression ratio and path length.
- Reject path traversal and symlinks unless explicitly supported safely.
- Verify hashes/signatures.
- Scan dependencies.
- Never execute package content during validation.
- Keep signing private keys server-side.
- Document reproducible build expectations.

## Naming conventions
Use stable IDs independent of display names. Use semantic versions for runtime and package compatibility, and immutable IDs for exact releases.
