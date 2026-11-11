# 13 — Package Formats and Build Pipeline

- `.pk`: editable Studio project/share.
- `.pkx`: player game package.
- `.pkxx`: game-specific server package.

Shared Dedicated Server Runtime is installed separately; `.pkxx` contains game-specific server content/configuration, not a duplicate runtime.

Manifest: game/release IDs, format, runtime compatibility, MUTE version, platforms, networking modes, labels, permissions, dependencies, entry points, limits and hashes.

Build: validate project, resolve dependencies, validate MUTE, convert assets, build client/server packages, generate manifest/hashes, sign server-side, verify artifacts.

Protect against path traversal, oversized archives, compression bombs, tampering and embedded secrets.

Exit gate: CLI builds/verifies/inspects package types without Studio UI.
