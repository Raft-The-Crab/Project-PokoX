# physics

Rigid-body world seam (`api::IPhysicsWorld`). Two implementations, deliberately
separate targets so a MOCK can never be confused with production
(systemplan/30 rule 12):

- `poko_physics_null` — **MOCK** `NullPhysicsWorld`, header
  `include/poko/physics/world.h`. Test-only fake: gravity integration and
  analytic raycasts, no collision response.
- `poko_physics_custom` — **PRODUCTION** `CustomPhysicsWorld`, header
  `include/poko/physics/custom_world.h`. Confirmed by systemplan/29
  ("custom physics"). Deterministic fixed-substep simulation, C++20 standard
  library only.

`CustomPhysicsWorld` pipeline per `PhysicsWorldDesc::fixed_delta_seconds`
substep (at most 8 per `step`):

1. Gravity and light damping for awake dynamic bodies.
2. Sweep-and-prune broadphase (x sort, y/z overlap), layer/mask filtering,
   deterministic pair ordering.
3. Exact narrow phase (`src/narrowphase.*`) for sphere, capsule (local Y axis)
   and oriented box: SAT with face clipping and edge-edge contacts for
   box-box, multi-point manifolds for capsules and boxes, plus a ground plane
   (`set_ground`) giving per-vertex box contacts.
4. Joint rows (ball, fixed, hinge, slider; limits and motors), contact warm
   starting (contacts matched to the previous step by proximity), then linear
   position correction (slop 5 mm, tracked per body so manifolds are
   not over-corrected), then 12 sequential-impulse iterations with angular
   terms (per-shape inertia tensors), two-tangent Coulomb friction and
   restitution above a 1 m/s threshold.
5. Island sleeping: a connected group of dynamic bodies under 5 cm/s for
   0.5 s sleeps; contact with a moving body, `wake()` or `set_body_state()`
   wakes it. Introspection: `is_sleeping`, `sleeping_body_count`,
   `contact_count`, `substep_count`.

Raycasts are exact against oriented spheres, boxes and capsules.

Rest damping removes residual sway from slow touching bodies, so tall stacks
settle and sleep. Joints are created with `create_joint` (world-space anchor and
axis), driven with `set_joint_motor`, read with `joint_position`; jointed bodies
do not collide unless `collide_connected`, share one sleep island and are
destroyed with their bodies.

**Not yet done:** continuous collision detection for fast
small bodies, convex-hull and triangle-mesh shapes, triggers and events, shape
casts, spring and cone joints.

Public headers live under `include/poko/physics/`; sources under `src/`;
tests under `tests/`.
