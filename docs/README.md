# Documentation

> **Project**: common_game_server — Tier 3 application of the [kcenon ecosystem](ECOSYSTEM.md)
> **Version**: see [`../VERSION`](../VERSION)
> **English** · [한국어](../README.kr.md)

This index lists all active documentation. Historical SDLC documents are
preserved under [`archive/sdlc/`](archive/sdlc/) for reference.

## Core Documents

| Document | Description |
|---|---|
| [`GETTING_STARTED.md`](GETTING_STARTED.md) | 5-10 minute tutorial for new users |
| [`FEATURES.md`](FEATURES.md) · [한](FEATURES.kr.md) | Complete feature matrix |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) · [한](ARCHITECTURE.kr.md) | System design, layer model, components |
| [`API_REFERENCE.md`](API_REFERENCE.md) · [한](API_REFERENCE.kr.md) | Public API reference |
| [`API_QUICK_REFERENCE.md`](API_QUICK_REFERENCE.md) | 1-page cheat sheet |
| [`BENCHMARKS.md`](BENCHMARKS.md) · [한](BENCHMARKS.kr.md) | Performance metrics and analysis |
| [`PROJECT_STRUCTURE.md`](PROJECT_STRUCTURE.md) · [한](PROJECT_STRUCTURE.kr.md) | Directory layout explanation |
| [`PRODUCTION_QUALITY.md`](PRODUCTION_QUALITY.md) · [한](PRODUCTION_QUALITY.kr.md) | SLA, readiness checklist |
| [`ROADMAP.md`](ROADMAP.md) | Implementation milestones |
| [`CHANGELOG.md`](CHANGELOG.md) · [한](CHANGELOG.kr.md) | Release history (mirrors root [`../CHANGELOG.md`](../CHANGELOG.md)) |

## Ecosystem & Compliance

| Document | Description |
|---|---|
| [`ECOSYSTEM.md`](ECOSYSTEM.md) | Position in the kcenon ecosystem (Tier 3) |
| [`COMPATIBILITY.md`](COMPATIBILITY.md) | C++ standard, compiler, and platform support |
| [`DEPRECATION.md`](DEPRECATION.md) | Deprecated APIs and removal timeline |
| [`TRACEABILITY.md`](TRACEABILITY.md) | Cross-document reference map |
| [`SOUP.md`](SOUP.md) | Software of Unknown Pedigree (third-party BOM) |

## How-to Guides

| Guide | Description |
|---|---|
| [`guides/BUILD_GUIDE.md`](guides/BUILD_GUIDE.md) | Conan + CMake walkthrough |
| [`guides/DEPLOYMENT_GUIDE.md`](guides/DEPLOYMENT_GUIDE.md) | Production deployment (Docker, Kubernetes) |
| [`guides/CONFIGURATION_GUIDE.md`](guides/CONFIGURATION_GUIDE.md) | YAML config, env vars, hot reload |
| [`guides/TESTING_GUIDE.md`](guides/TESTING_GUIDE.md) | Unit, integration, load, chaos testing |
| [`guides/PLUGIN_DEVELOPMENT_GUIDE.md`](guides/PLUGIN_DEVELOPMENT_GUIDE.md) | Authoring custom game plugins |
| [`guides/INTEGRATION_GUIDE.md`](guides/INTEGRATION_GUIDE.md) | Integrating with downstream projects |
| [`guides/TROUBLESHOOTING.md`](guides/TROUBLESHOOTING.md) | Common issues and solutions |
| [`guides/FAQ.md`](guides/FAQ.md) | Frequently asked questions |

## Tutorials (Doxygen pages)

Runnable tutorials live under `docs/tutorial_*.dox` and render into
the published Doxygen site. Each has a companion program under
[`../examples/`](../examples/) that you can build with
`cmake --preset debug`.

**Framework basics**

| Tutorial | Topic |
|---|---|
| `tutorial_ecs.dox` | Entity-Component System walkthrough |
| `tutorial_plugin.dox` | Plugin authoring skeleton |
| `tutorial_service.dox` | Running and configuring microservices |

**Core patterns**

| Tutorial | Topic |
|---|---|
| `tutorial_result_errors.dox` | `Result<T>`, `GameError`, categorized error codes |
| `tutorial_foundation_adapters.dox` | Foundation adapters and `ServiceLocator` |

**Game logic**

| Tutorial | Topic |
|---|---|
| `tutorial_game_objects.dox` | `Transform`, `Identity`, `Stats`, `Movement` |
| `tutorial_spatial_world.dox` | `SpatialIndex`, zones, and interest management |
| `tutorial_combat.dox` | Combat system, damage types, auras, spell casting |
| `tutorial_ai_behavior.dox` | AI behavior trees and blackboards |
| `tutorial_inventory.dox` | Inventory and equipment slots |
| `tutorial_quest.dox` | Quest state machine and objectives |

**Persistence and networking**

| Tutorial | Topic |
|---|---|
| `tutorial_database.dox` | `GameDatabase`, prepared statements, transactions |
| `tutorial_networking.dox` | Server-side `NetworkMessage`, signals, TLS |
| `tutorial_client.dox` | Client-side loopback round-trip with `kcenon::network` facades |

See [`../examples/README.md`](../examples/README.md) for the
corresponding runnable programs.

## Advanced Topics

| Document | Description |
|---|---|
| [`advanced/ECS_DEEP_DIVE.md`](advanced/ECS_DEEP_DIVE.md) | Entity-Component System internals |
| [`advanced/FOUNDATION_ADAPTERS.md`](advanced/FOUNDATION_ADAPTERS.md) | Adapter pattern reference |
| [`advanced/PROTOCOL_SPECIFICATION.md`](advanced/PROTOCOL_SPECIFICATION.md) | Network protocol specification |
| [`advanced/DATABASE_SCHEMA.md`](advanced/DATABASE_SCHEMA.md) | Database design |

## Architecture Decision Records

| ADR | Title |
|---|---|
| [`adr/ADR-001-unified-game-server-architecture.md`](adr/ADR-001-unified-game-server-architecture.md) | Why merge four legacy projects |
| [`adr/ADR-002-entity-component-system.md`](adr/ADR-002-entity-component-system.md) | ECS over OOP |
| [`adr/ADR-003-plugin-hot-reload.md`](adr/ADR-003-plugin-hot-reload.md) | Plugin hot-reload model |
| [`adr/ADR-004-microservice-decomposition.md`](adr/ADR-004-microservice-decomposition.md) | 5-service decomposition |

## Contributing

| Document | Description |
|---|---|
| [`contributing/CODING_STANDARDS.md`](contributing/CODING_STANDARDS.md) | C++ style guidelines |
| [`contributing/DOCUMENTATION_GUIDELINES.md`](contributing/DOCUMENTATION_GUIDELINES.md) | How to write project docs |
| [`contributing/CI_CD_GUIDE.md`](contributing/CI_CD_GUIDE.md) | CI/CD workflow guide |
| [`contributing/CHANGELOG_TEMPLATE.md`](contributing/CHANGELOG_TEMPLATE.md) | Changelog entry template |
| [`contributing/templates/`](contributing/templates/) | Markdown templates |

## Performance

| Document | Description |
|---|---|
| [`performance/BENCHMARKS_METHODOLOGY.md`](performance/BENCHMARKS_METHODOLOGY.md) | How benchmarks are measured |

## Archive

| Document | Description |
|---|---|
| [`archive/README.md`](archive/README.md) | Archive overview |
| [`archive/sdlc/`](archive/sdlc/) | Legacy SDLC documents (PRD, SRS, SDS, etc.) |

---

*See [`../README.md`](../README.md) for the project overview and quick start.*
