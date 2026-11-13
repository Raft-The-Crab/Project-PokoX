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

Continuous collision detection (on by default, `set_ccd_enabled`): a body whose
surface moves more than half its inner radius per step (linear speed plus spin
* bounding radius) is advanced to the exact time of impact against every
other body whose swept bounds overlap its own. The test is shape against shape
(GJK distance on sphere/capsule/box cores plus conservative advancement), so
rotation of both bodies is included and corners and edges are exact, with no
inflated-box approximation. The body moves only that fraction of the step and
stops about 2 mm inside contact, so the regular solver resolves the hit,
restitution included. Only fast bodies are tested, each after a cheap swept-AABB
reject. `ccd_hit_count()` reports activations. The ground plane needs no CCD
because it is a half-space.

Robustness: non-finite or zero-quaternion input is rejected or sanitized, the
fixed step must lie in 0.1-100 ms, linear and angular speed are capped (1000 m/s,
300 rad/s), per-body push-out is capped at 0.2 m per step so deep overlaps
resolve over several steps, work beyond the 8-substep cap is dropped instead of
accumulating, moving kinematic bodies wake sleeping bodies they touch, and a
body that still goes non-finite is restored to its last finite state
(`nonfinite_recoveries()` counts these and should stay 0).

Rest damping removes residual sway from slow touching bodies, so tall stacks
settle and sleep. Joints are created with `create_joint` (world-space anchor and
axis), driven with `set_joint_motor`, read with `joint_position`; jointed bodies
do not collide unless `collide_connected`, share one sleep island and are
destroyed with their bodies.

**Not yet done:** 
convex-hull and triangle-mesh shapes, triggers and events, shape
casts, spring and cone joints.

Public headers live under `include/poko/physics/`; sources under `src/`;
tests under `tests/`.
