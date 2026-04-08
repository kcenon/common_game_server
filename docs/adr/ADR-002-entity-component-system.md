---
doc_id: "CGS-ADR-002"
doc_title: "ADR-002: Entity-Component System over OOP Hierarchies"
doc_version: "1.0.0"
doc_date: "2026-04-08"
doc_status: "Accepted"
project: "common_game_server"
category: "ADR"
---

# ADR-002: Entity-Component System over OOP Hierarchies

> **SSOT**: This document is the single source of truth for **ADR-002**.

| Field | Value |
|-------|-------|
| Status | Accepted |
| Date | 2026-02-03 |
| Decision Makers | kcenon ecosystem maintainers |

## Context

Game servers need to manage thousands of entities (players, NPCs, projectiles,
loot drops, etc.) with mixed sets of behaviors. The two dominant approaches:

1. **OOP hierarchies** — `Entity` base class with subclasses (`Player`,
   `Monster`, `Projectile`) and overridden virtual methods.
2. **Entity-Component System (ECS)** — Entities are integer IDs.
   Behavior is encoded by adding/removing components. Systems iterate over
   entities with specific component combinations.

Three of the four legacy projects used OOP hierarchies. They suffered from:

- **Diamond inheritance** — A `MountedArcher` needed both `Mountable` and
  `RangedAttacker` traits, leading to virtual inheritance gymnastics
- **Cache misses** — Polymorphic dispatch and scattered allocations defeated
  CPU prefetchers
- **Rigid composition** — Adding a temporary effect (e.g., "frozen") required
  modifying the base class or using flag enums
- **Hard-to-parallelize** — Iterating heterogeneous entities serially

## Decision

**Adopt a data-oriented Entity-Component System as the core entity model.**

Specifically:

1. **Entities are 64-bit IDs** with version recycling. No `Entity` class.
2. **Components are POD-like structs** stored in `SparseSet<T>` pools — one
   pool per component type. SoA layout, cache-friendly iteration.
3. **Systems** are functions/classes that iterate over `Query<Components...>`
   results. Systems run via the `SystemScheduler` with DAG dependencies and
   parallel execution where allowed.
4. **No inheritance** at the entity level. Composition via components only.
5. **Compile-time type safety** — Queries use C++20 concepts to enforce
   component constraints.

## Alternatives Considered

### OOP hierarchies

- **Pros**: Familiar to many developers; intuitive for small object counts.
- **Cons**: All the issues listed in Context above. Does not scale to 10K+ entities.

### Hybrid (OOP for "characters", ECS for "particles")

- **Pros**: Migrate gradually; familiar code stays.
- **Cons**: Two parallel systems to maintain; the boundary becomes a constant
  source of confusion. Three of the four legacy projects tried this and ended
  up rewriting.

### Existing ECS library (entt, EnTT, flecs)

- **Pros**: Battle-tested, feature-rich.
- **Cons**: Adds an external dependency outside the kcenon ecosystem;
  performance characteristics need re-validation; integration with the
  kcenon foundation systems requires custom wrappers anyway.

## Consequences

### Positive

- **Data-oriented performance** — Cache-friendly iteration, predictable
  memory access patterns
- **10K entities < 5 ms tick** — Verified by benchmarks (see [`../BENCHMARKS.md`](../BENCHMARKS.md))
- **Trivial composition** — Adding a "frozen" effect = adding a `Frozen`
  component. No base class modifications.
- **Parallel systems** — DAG scheduler runs independent systems concurrently
- **Compile-time safety** — Queries are type-checked at compile time

### Negative

- **Conceptual learning curve** — Developers used to OOP need time to "think in components"
- **Debugging is harder** — Stepping through ECS iteration is less intuitive than stepping through method calls
- **No "Entity" object** — Cannot pass `Entity*` around; must always pass `EntityID + EntityManager*`
- **Component design discipline** — Components must remain POD-like for cache
  efficiency; mixing heavy logic into components defeats the purpose

## Implementation Notes

- `EntityManager` provides O(1) entity create/destroy with version recycling
- `ComponentPool<T>` uses `SparseSet` for O(1) add/remove/lookup
- `Query<Components...>` is a compile-time abstraction with `each` and `par_each`
- `SystemScheduler` builds a DAG from declared dependencies (`depends_on<...>`)

Detailed internals: [`../advanced/ECS_DEEP_DIVE.md`](../advanced/ECS_DEEP_DIVE.md).

## References

- [`../advanced/ECS_DEEP_DIVE.md`](../advanced/ECS_DEEP_DIVE.md) — Internals
- [`../ARCHITECTURE.md#layer-4---core-ecs-layer`](../ARCHITECTURE.md) — Layer position
- [`../BENCHMARKS.md`](../BENCHMARKS.md) — Performance verification
- Mike Acton: "Data-Oriented Design and C++" (CppCon 2014)
- entt design philosophy: https://github.com/skypjack/entt
