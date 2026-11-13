# ecs

Entity-component registry: generation-checked entity handles, per-type sparse-set
component pools (deterministic insertion-order iteration, swap-remove) and
multi-component queries.

- `add_component` returns false for dead or stale entities; stale handles never
  see a recycled slot's components.
- `each<A, B...>(fn)` calls `fn(Entity, A&, B&...)` for every entity owning all
  listed components, driven by the smallest pool. It iterates a snapshot, so
  callbacks may add/remove components or create/destroy entities: entities that
  die or stop matching are skipped, entities added mid-pass are not visited.
- `entities_with<A, B...>()` returns the matching entities; `count<T>()` the
  size of one pool.
- Every method is synchronised; pointers from `get_component` are valid only
  until the next structural change.

Not yet done: archetype storage, change detection, system scheduling.

Public headers live under `include/poko/ecs/`; sources under `src/`;
tests under `tests/`.
