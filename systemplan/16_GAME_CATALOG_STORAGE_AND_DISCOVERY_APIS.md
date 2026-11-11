# 16 — Game Catalog, Storage and Discovery APIs

Catalog: game ID, owner/team, metadata, media, approved release, labels, platforms, modes, visibility and status.

Store immutable package blobs in object storage and metadata in database. Verify hashes, sign delivery manifests, define retention and deletion.

Discovery APIs: search, categories, featured, recently updated, favorites, history and creator pages. Start with simple transparent ranking.

Only approved release IDs are publicly playable. Pending updates do not replace the current approved release.

Exit gate: CLI can create game, upload, publish approved version, search and download/verify exact package.
