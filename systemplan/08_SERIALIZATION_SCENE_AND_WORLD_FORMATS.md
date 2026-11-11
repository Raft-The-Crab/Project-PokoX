# 08 — Serialization, Scene and World Formats

Define scene hierarchy, entity/component data, stable IDs, asset references, block/world representation, character/animation references, scripts, service declarations and project metadata.

Requirements: schema versions, migration paths, unknown-field policy, input bounds, corruption handling, deterministic serialization where practical and no executable deserialization hooks.

Test round trips, old-schema migration, truncated files, malformed values, oversized input and missing assets.

Exit gate: scenes save/load/migrate/validate without Studio UI.
