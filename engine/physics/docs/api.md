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
   layer/mask and sleep state. Triggers are excluded.
3. **Narrow phase** (`custom_collide.cpp`) — rotated shapes throughout:
   box–box via SAT with face manifolds (up to 8 points) or an edge–edge point,
   capsule/sphere via segment distance (a sphere is a zero-length capsule),
   capsule–box via sphere–box tests along the segment, plus the ground plane
   (`set_ground`, default y = 0). Contacts are **speculative** within 4 cm: a
   not-yet-touching pair may close at most its remaining gap per step.
4. **Solver** — 12 sequential-impulse iterations with accumulated normal and
   two friction impulses (Coulomb cone), restitution above 1 m/s approach speed,
   and a per-shape inertia tensor (cylinder + hemispheres for capsules) so
   contacts produce torque. Impulses are **warm-started** from the previous
   substep (matched by body pair and contact point within 5 cm). Joints are
   solved inside the same iteration loop.
5. **Continuous collision** — a dynamic body that would travel more than half
   its smallest dimension in one substep is swept (conservative advancement)
   against every other collider and the ground, and stopped 1 cm short of the
   first one it is moving into.
6. Integrate positions/rotations, then 2 positional projection passes that
   remove residual penetration beyond a 5 mm slop (once per body pair, by its
   deepest point).
7. **Sleeping** — a dynamic body below 0.05 m/s and 0.05 rad/s for 0.5 s
   sleeps. It wakes when a moving body touches it, on `set_body_state`, when a
   joint is created or destroyed, or when any body is destroyed. Bodies attached
   to a joint never sleep.

Kinematic bodies integrate their own velocity, are never pushed, and push
dynamic bodies.

### Joints

`create_joint(JointDesc)` / `destroy_joint` (outside the seam). Kinds:
`Ball` (anchors coincide), `Distance` (rod of fixed length), `Hinge` (anchors
coincide and axes stay aligned), `Fixed` (anchors coincide and relative
rotation locked). Anchors are in each body's local space; leave `body_b`
invalid to pin to the world, in which case `anchor_b`/`axis_b` are world space.
Position error is corrected with Baumgarte feedback (beta 0.2). A joint needs at
least one dynamic body and is destroyed with either of its bodies.

### Triggers

`BodyDesc::is_trigger` (Static or Kinematic only) makes a non-colliding volume.
After each `step`, `drain_trigger_events` yields `TriggerEvent`s when a
non-static, non-trigger body starts or stops overlapping it (layer/mask apply).
Triggers are invisible to raycasts and sweeps.

### Queries

- `raycast` (seam) — exact against rotated boxes, capsules and spheres; ignores
  the ground plane and triggers.
- `shape_cast(shape, position, rotation, direction, max_distance, mask, out)` —
  sweeps any sphere/box/capsule and reports the first body or the ground
  (`SweepHit::ground`) with distance, point and a normal facing the cast shape.
  Already-overlapping shapes report distance 0.
- `overlap(shape, position, rotation, mask, out, capacity)` — bodies currently
  overlapping a shape; returns the total count.

Sweeps and CCD move the shape linearly and ignore rotation during the move.

### Introspection

`contact_count()` (contacts, including speculative ones, from the last `step`),
`substep_count()`, `sleeping_count()`, `is_sleeping()`, `joint_count()`.

### Known limits

- Static geometry is boxes, capsules and spheres plus one ground plane; there
  are no triangle meshes or heightfields.
- Joint limits and motors are not implemented, only the four constraint kinds.
- Stacks are accurate to a few millimetres but the solver is iterative, so very
  tall or very heavy-on-light stacks can still wobble.
- The broadphase rebuilds every substep and allocates scratch vectors; it is
  not yet tuned for thousands of bodies.

Public headers live under `include/poko/physics/`; sources under `src/`
(`custom_collide.*` narrow phase and distance functions, `custom_step.cpp`
broadphase/solver/CCD/sleep, `custom_joints.cpp`, `custom_query.cpp` sweeps,
overlap and triggers, `custom_math.h` small matrix helpers, `custom_detail.h`
ray helpers); tests under `tests/`.
