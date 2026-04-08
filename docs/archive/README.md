# Documentation Archive

This directory preserves historical documentation that is no longer part of the
active documentation set. The content is retained for reference, traceability,
and auditing purposes.

Active documentation follows the **kcenon ecosystem template** and lives under
[`../`](../) — start with [`../README.md`](../README.md) for the current
documentation index.

## Contents

| Directory | Description |
|-----------|-------------|
| [`sdlc/`](sdlc/) | Legacy SDLC documents (PRD, SRS, SDS, INTEGRATION_STRATEGY, INDEX, PERFORMANCE_REPORT) from the pre-ecosystem-alignment era. Their content has been migrated and refactored into the kcenon-style documentation set. |

## Why preserved?

These documents captured the original design intent, requirement decomposition,
and integration strategy when `common_game_server` was bootstrapped from four
legacy projects. While the active documentation now follows the kcenon
ecosystem template (FEATURES, ARCHITECTURE, API_REFERENCE, BENCHMARKS,
ECOSYSTEM), the historical record helps with:

- Understanding the original decisions and constraints
- Traceability from requirements → design → implementation
- Auditing changes introduced during the ecosystem alignment
- Referencing sections that were condensed or split in the active docs

## Where did the content go?

See [`../TRACEABILITY.md`](../TRACEABILITY.md) for the mapping from each
archived document section to its new location in the active documentation set.
