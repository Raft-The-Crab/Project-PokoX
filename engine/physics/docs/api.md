# physics

Rigid-body world seam (`api::IPhysicsWorld`). Two implementations, deliberately
separate targets so a MOCK can never be confused with production
(systemplan/30 rule 12):

- `poko_physics_null` — **MOCK** `NullPhysicsWorld`, header
  `include/poko/physics/world.h`. Test-only fake: gravity integration and
  analytic raycasts, no collision response.
- `poko_physics_custom` — **PRODUCTION** `CustomPhysicsWorld`, header
  `include/poko/physics/custom_world.h`. Confirmed by systemplan/29
  ("custom physics"). Deterministic fixed-substep simulation using only the
  C++20 standard library.

`CustomPhysicsWorld` runs `PhysicsWorldDesc::fixed_delta_seconds` substeps
(capped at 8 per `step`), integrates linear + angular state, resolves a static
ground plane at y = 0 (configurable via `set_ground`), and applies impulse-based
collision response with restitution/friction and layer/mask filtering. The
narrow phase uses conservative bounding spheres (box = circumscribed sphere,
capsule = radius + half_height); exact polyhedral contact and sleeping are
future work. Raycasts are analytic against sphere, AABB and capsule bounds.
`contact_count()` / `substep_count()` expose deterministic introspection.

Public headers live under `include/poko/physics/`; sources under `src/`
(`custom_detail.h` holds shared `detail::` helpers); tests under `tests/`.
