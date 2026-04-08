# Tutorial Examples

This directory contains runnable companion programs for every tutorial
under `docs/tutorial_*.dox`. Each `NN_<topic>.cpp` file is a
self-contained, end-to-end demonstration that you can build, run,
and modify while following along with the matching tutorial.

## Enabling the build

The examples are opt-in — they do not build as part of the default
preset. Enable them either through a preset or manually:

```bash
# Recommended: the debug preset enables CGS_BUILD_EXAMPLES automatically.
cmake --preset debug
cmake --build build-debug -j

# Or turn the flag on manually with any other preset:
cmake -B build -DCGS_BUILD_EXAMPLES=ON
cmake --build build -j
```

The `debug` and `ci` presets both set `CGS_BUILD_EXAMPLES=ON`, so CI
compiles every example on every PR and drift from the real API is
caught immediately.

## Running

Built binaries land in `${CMAKE_BINARY_DIR}/examples/`. For the
`debug` preset that's `build-debug/examples/`:

```bash
./build-debug/examples/cgs_example_01_result_errors
./build-debug/examples/cgs_example_03_game_objects
# ... etc
```

Every example exits with code 0 on success and prints its progress
to stdout, so they are safe to run inside CI without a database or
network peer.

## Building a single example

```bash
cmake --build build-debug --target cgs_example_05_combat
```

Or build all of them at once with the aggregate convenience target:

```bash
cmake --build build-debug --target cgs_examples
```

## What each example demonstrates

| # | File | Tutorial | What it shows |
|---|------|----------|---------------|
| 01 | `01_result_errors.cpp` | [`../docs/tutorial_result_errors.dox`](../docs/tutorial_result_errors.dox) | 3-stage config loader using `GameResult<T>` with manual error propagation and `valueOr` fallback |
| 02 | `02_foundation_adapters.cpp` | [`../docs/tutorial_foundation_adapters.dox`](../docs/tutorial_foundation_adapters.dox) | `GameLogger` with `LogContext`, custom `IAchievementTracker` interface, `ServiceLocator` DI |
| 03 | `03_game_objects.cpp` | [`../docs/tutorial_game_objects.dox`](../docs/tutorial_game_objects.dox) | Spawn 100 NPCs in a grid, attach Transform/Identity/Stats/Movement, tick with `ObjectUpdateSystem` |
| 04 | `04_spatial_world.cpp` | [`../docs/tutorial_spatial_world.dox`](../docs/tutorial_spatial_world.dox) | Scatter 200 entities, perform radius queries before and after movement, `ZoneFlags` composition |
| 05 | `05_combat.cpp` | [`../docs/tutorial_combat.dox`](../docs/tutorial_combat.dox) | Player casts Fireball on a boss, fire resistance mitigation, 3-tick burning aura via `CombatSystem` |
| 06 | `06_ai_behavior.cpp` | [`../docs/tutorial_ai_behavior.dox`](../docs/tutorial_ai_behavior.dox) | Patrol-aggro-flee behavior tree with `BTSelector` + `BTSequence` + `BTCondition` + `BTAction` |
| 07 | `07_inventory.cpp` | [`../docs/tutorial_inventory.dox`](../docs/tutorial_inventory.dox) | 3-item catalog, add/equip/split/unequip, compute equipped stat bonuses |
| 08 | `08_quest.cpp` | [`../docs/tutorial_quest.dox`](../docs/tutorial_quest.dox) | Accept a 2-objective wolf quest, advance progress, turn in, verify re-accept is blocked |
| 09 | `09_database.cpp` | [`../docs/tutorial_database.dox`](../docs/tutorial_database.dox) | `PreparedStatement` name binding, `DatabaseConfig`, `GameResult<void>` connect-error handling |
| 10 | `10_networking.cpp` | [`../docs/tutorial_networking.dox`](../docs/tutorial_networking.dox) | `NetworkMessage` serialize / deserialize round-trip, `Signal<SessionId>` connect/emit/disconnect |

## Examples that skip I/O

Two examples (`09_database` and `10_networking`) deliberately stop
short of real I/O because they would otherwise require a running
PostgreSQL instance or a live network peer. Each exercises every
offline part of the API (construction, configuration, parameter
binding, signal dispatch) and the companion tutorials show the
online usage patterns in source snippets. Live-network and
live-database coverage lives in the integration test suite under
`tests/`.

## Authoring new examples

1. Add a new `NN_topic.cpp` file following the existing style
   (self-contained, zero external dependencies beyond the CGS
   targets, exits 0 on success).
2. Add a `cgs_add_example(cgs_example_NN_topic NN_topic.cpp)` call
   to `CMakeLists.txt` plus the per-example
   `target_link_libraries` with only the CGS libraries your example
   actually uses.
3. Append the new target to the `cgs_examples` aggregate custom
   target so `cmake --build --target cgs_examples` stays in sync.
4. Write a matching `docs/tutorial_<topic>.dox` (300+ lines) and
   add it to `docs/mainpage.dox` as a `@subpage` entry — **never**
   use `@subpage` inside another tutorial, only inside `mainpage.dox`.
   See `docs/contributing/DOCUMENTATION_GUIDELINES.md` for the
   required section pattern.
5. Update this README's table.

## See also

- [`../docs/mainpage.dox`](../docs/mainpage.dox) — Doxygen index and
  subpage listing
- [`../docs/contributing/DOCUMENTATION_GUIDELINES.md`](../docs/contributing/DOCUMENTATION_GUIDELINES.md)
  — tutorial style guide and section template
- [`../docs/ARCHITECTURE.md`](../docs/ARCHITECTURE.md) — high-level
  architecture for context before diving into a specific tutorial
