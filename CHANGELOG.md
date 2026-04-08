# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Align documentation with the kcenon ecosystem template (epic [#122](https://github.com/kcenon/common_game_server/issues/122))
- Bilingual core documentation (English + Korean) for README, ARCHITECTURE, FEATURES, API_REFERENCE, BENCHMARKS, PROJECT_STRUCTURE, PRODUCTION_QUALITY, CHANGELOG
- Root governance files: `CLAUDE.md`, `CODE_OF_CONDUCT.md`, `CONTRIBUTING.md`, `SECURITY.md`, `VERSION`, `VERSIONING.md`, `LICENSE-THIRD-PARTY`, `NOTICES`, `DEPENDENCY_MATRIX.md`, `codecov.yml`, `CMakePresets.json`
- `docs/ECOSYSTEM.md` — positions `common_game_server` as Tier 3 application in the kcenon ecosystem
- `docs/TRACEABILITY.md` — cross-document reference map
- `docs/SOUP.md` — third-party Bill of Materials
- Architecture Decision Records under `docs/adr/` (4 ADRs)
- Doxygen integration: `mainpage.dox`, `faq.dox`, `troubleshooting.dox`, `tutorial_*.dox`, doxygen-awesome theme

### Changed

- Documentation structure migrated from SDLC layout (PRD/SRS/SDS) to kcenon
  product-doc layout (FEATURES/ARCHITECTURE/API_REFERENCE/BENCHMARKS) — legacy
  documents preserved under `docs/archive/sdlc/`
- `docs/reference/` split into `docs/guides/`, `docs/advanced/`, `docs/contributing/`
- `Doxyfile` rewritten to match `common_system` layout, with theme assets

### Removed

- `docs/reference/` directory (contents redistributed to `guides/`, `advanced/`, `contributing/`)

## [0.1.0] - 2026-02-03

### Added

- Initial unified game server framework combining four legacy projects
- 6-layer architecture: Foundation → Adapters → Services → ECS → Game Logic → Plugins
- Entity-Component System with `SparseSet` storage, parallel execution, query API
- 5 microservices: Auth (JWT RS256), Gateway, Game, Lobby (ELO), DBProxy
- 7 foundation adapters wrapping the kcenon ecosystem (common, thread, logger, network, database, container, monitoring)
- Game logic systems: Object, Combat, World, AI (BehaviorTree), Quest, Inventory
- Plugin manager with hot reload, dependency resolution, event communication
- MMORPG plugin example
- WAL + snapshots, circuit breaker, graceful shutdown
- Kubernetes deployment manifests (HPA, StatefulSet, PDB)
- Prometheus + Grafana monitoring stack
- Doxygen API documentation generation
- GitHub Actions CI: lint, build/test (3 configs), coverage, docs, benchmarks, load test, chaos test

### Infrastructure

- CMake 3.20+ with Conan 2 package management
- clang-format 21 (Google-based, 4-space indent, 100-column limit)
- clang-tidy static analysis
- BSD-3-Clause license
