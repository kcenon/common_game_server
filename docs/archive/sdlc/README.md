# SDLC Archive

This directory contains the original Software Development Life Cycle documents
that guided the initial construction of `common_game_server`. They are preserved
verbatim from the pre-ecosystem-alignment era.

> **Note**: These documents are **not** the active source of truth. Refer to
> the kcenon-style documentation in [`../../`](../../) for current information.

## Documents

| File | Lines | Original purpose |
|------|-------|------------------|
| [`PRD.md`](PRD.md) | 307 | Product Requirements Document — vision, objectives, success metrics, functional/non-functional requirements, tech stack constraints, milestones |
| [`SRS.md`](SRS.md) | 1,285 | Software Requirements Specification — 126 requirements traced from PRD with functional and non-functional decomposition |
| [`SDS.md`](SDS.md) | 2,345 | Software Design Specification — 29 modules, detailed architecture, component design, API specifications, deployment architecture |
| [`INTEGRATION_STRATEGY.md`](INTEGRATION_STRATEGY.md) | 834 | Integration Strategy — how content from four legacy projects was merged into a unified framework, layer mapping, hybrid bridge patterns |
| [`INDEX.md`](INDEX.md) | 374 | Legacy documentation index |
| [`PERFORMANCE_REPORT.md`](PERFORMANCE_REPORT.md) | 324 | Benchmark results and analysis |

## Migration mapping

| Legacy document | Migrated to |
|-----------------|-------------|
| `PRD.md` | [`../../FEATURES.md`](../../FEATURES.md), [`../../ROADMAP.md`](../../ROADMAP.md) |
| `SRS.md` | [`../../FEATURES.md`](../../FEATURES.md), [`../../ARCHITECTURE.md`](../../ARCHITECTURE.md), ADRs |
| `SDS.md` | [`../../ARCHITECTURE.md`](../../ARCHITECTURE.md), [`../advanced/`](../../advanced/) |
| `INTEGRATION_STRATEGY.md` | [`../../ECOSYSTEM.md`](../../ECOSYSTEM.md), [`../guides/INTEGRATION_GUIDE.md`](../../guides/INTEGRATION_GUIDE.md), [`../../adr/ADR-001-unified-game-server-architecture.md`](../../adr/ADR-001-unified-game-server-architecture.md) |
| `INDEX.md` | [`../../README.md`](../../README.md) |
| `PERFORMANCE_REPORT.md` | [`../../BENCHMARKS.md`](../../BENCHMARKS.md), [`../performance/BENCHMARKS_METHODOLOGY.md`](../../performance/BENCHMARKS_METHODOLOGY.md) |

Full cross-reference map: [`../../TRACEABILITY.md`](../../TRACEABILITY.md)
