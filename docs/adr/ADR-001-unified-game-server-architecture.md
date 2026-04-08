---
doc_id: "CGS-ADR-001"
doc_title: "ADR-001: Unified Game Server Architecture"
doc_version: "1.0.0"
doc_date: "2026-04-08"
doc_status: "Accepted"
project: "common_game_server"
category: "ADR"
---

# ADR-001: Unified Game Server Architecture

> **SSOT**: This document is the single source of truth for **ADR-001: Unified Game Server Architecture**.

| Field | Value |
|-------|-------|
| Status | Accepted |
| Date | 2026-02-03 |
| Decision Makers | kcenon ecosystem maintainers |

## Context

`common_game_server` was bootstrapped from four legacy game server projects,
each with overlapping features but inconsistent APIs, error models, and build
systems. Maintaining four parallel codebases imposed:

- **Duplicated effort** — bug fixes had to be ported across four projects
- **Inconsistent quality** — each project had different test coverage and CI
- **Integration friction** — combining features from two projects required manual porting
- **No shared ecosystem benefits** — only one project consumed the kcenon foundation libraries

The four legacy projects shared a similar architectural skeleton (network
gateway → game world → database) but differed in:
- ECS vs OOP entity model
- Plugin API (or lack thereof)
- Error handling (exceptions vs error codes vs Result<T>)
- Build system (CMake/Conan vs Meson vs raw Makefile)

## Decision

**Merge the four legacy projects into a single unified framework
(`common_game_server`) built on the kcenon ecosystem.**

Specifically:

1. **Single repository** — `kcenon/common_game_server` becomes the canonical
   game server framework. Legacy repos are archived (read-only).
2. **Layered architecture** — 6 layers (foundation → adapters → services →
   ECS → game logic → plugins) with strict downward dependencies.
3. **kcenon ecosystem foundation** — All foundation needs (logging, network,
   database, threading, monitoring, container, common utilities) are
   delegated to the kcenon Tier 0-2 systems via thin adapter interfaces.
4. **`Result<T, E>` error model** — No exceptions cross layer or plugin
   boundaries. Inherited from `kcenon::common::Result<T>`.
5. **CMake + Conan** — Single build system. CMake 3.20+ with presets,
   Conan 2 for transitive dependencies.

## Alternatives Considered

### Keep legacy projects separate

- **Pros**: No migration cost; existing users not disrupted.
- **Cons**: Continued duplication; each project must individually adopt
  kcenon ecosystem improvements; combined feature set is impossible.

### Pick one legacy project as the base, deprecate the others

- **Pros**: Lower migration cost than full rewrite.
- **Cons**: Forces choosing a "winner" — the chosen project may lack the
  best features of the others. Harder to justify abandoning user investment
  in deprecated projects.

### Rewrite from scratch

- **Pros**: Clean slate, no legacy baggage.
- **Cons**: Throws away all battle-tested code; lengthy "second-system effect"
  risk; ignores valuable patterns proven by the legacy projects.

## Consequences

### Positive

- **Single source of truth** — One codebase, one CI, one release cadence
- **Layered cleanliness** — Strict layer separation prevents the layer-violation
  cruft that accumulated in legacy projects
- **Ecosystem leverage** — All foundation improvements (logging, network, etc.)
  are inherited automatically from kcenon upstream
- **Consistent error handling** — `Result<T, E>` everywhere; no more mixing
  exceptions and error codes
- **Plugin extensibility** — Legacy game-specific code is migrated into
  plugins, keeping the framework game-agnostic

### Negative

- **Migration cost** — Existing users of legacy projects need to port their
  code. Mitigated by providing migration guides under `docs/guides/`.
- **Dependency on kcenon ecosystem** — `common_game_server` cannot ship
  ahead of kcenon foundation releases. Mitigated by version pinning.
- **Larger surface area** — A single repo with 6 layers is a bigger codebase
  than any individual legacy project. Mitigated by clear layer documentation.

## Migration Plan

The original migration strategy is preserved in
[`../archive/sdlc/INTEGRATION_STRATEGY.md`](../archive/sdlc/INTEGRATION_STRATEGY.md)
for historical reference.

## References

- [`../ARCHITECTURE.md`](../ARCHITECTURE.md) — Layer model and topology
- [`../ECOSYSTEM.md`](../ECOSYSTEM.md) — Position in the kcenon ecosystem
- [`../archive/sdlc/INTEGRATION_STRATEGY.md`](../archive/sdlc/INTEGRATION_STRATEGY.md) — Original integration strategy (archived)
- [`ADR-002-entity-component-system.md`](ADR-002-entity-component-system.md) — ECS choice
- [`ADR-004-microservice-decomposition.md`](ADR-004-microservice-decomposition.md) — Service split
