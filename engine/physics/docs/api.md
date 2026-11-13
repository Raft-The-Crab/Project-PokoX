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

## CustomPhysicsWorld

Units: metres, kilograms, seconds; right-handed, +Y up (see
`physics_contracts.h`). `step` runs `PhysicsWorldDesc::fixed_delta_seconds`
substeps (capped at 8 per call, frame delta capped at 0.25 s).

Per substep:

1. Integrate gravity into awake dynamic bodies (small angular damping).
2. **Broadphase** — sweep-and-prune on the X axis over world AABBs, filtered by
   layer/mask and sleep state.
3. **Narrow phase** (`custom_collide.cpp`) — rotated shapes throughout:
   box–box via SAT with face manifolds (up to 8 points) or an edge–edge point,
   capsule/sphere via segment distance (a sphere is a zero-length capsule),
   capsule–box via sphere–box tests along the segment, plus the ground plane
   (`set_ground`, default y = 0).
4. **Solver** — 12 sequential-impulse iterations with accumulated normal and
   two friction impulses (Coulomb cone), restitution above 1 m/s approach speed,
   and a per-shape inertia tensor so contacts produce torque. No warm-starting.
5. Integrate positions/rotations, then 2 positional projection passes that
   remove residual penetration beyond a 5 mm slop (once per body pair, by its
   deepest point).
6. **Sleeping** — a dynamic body below 0.05 m/s and 0.05 rad/s for 0.5 s
   sleeps. It wakes when a moving body touches it, on `set_body_state`, or when
   any body is destroyed.

Kinematic bodies integrate their own velocity, are never pushed, and push
dynamic bodies. Capsule inertia uses the capsule's bounding box. Raycasts are
exact against rotated boxes, capsules and spheres (ground plane excluded).

Known limits (tracked for later phases): no continuous collision detection
(fast thin bodies can tunnel), no joints, no triggers, no sweeps/overlap
queries, stacks sag a few millimetres and tall stacks may not sleep because
the solver is not warm-started.

Introspection outside the seam: `contact_count()`, `substep_count()`,
`sleeping_count()`, `is_sleeping()`.

Public headers live under `include/poko/physics/`; sources under `src/`
(`custom_collide.*` narrow phase, `custom_detail.h` ray helpers,
`custom_step.cpp` broadphase/solver/sleep); tests under `tests/`.
