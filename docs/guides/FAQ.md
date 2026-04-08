# Frequently Asked Questions

For Doxygen-rendered FAQ, see [`../faq.dox`](../faq.dox).

## General

### What is `common_game_server`?

A unified C++20 game server framework combining an Entity-Component System
(ECS), a hot-reloadable plugin architecture, and 5 horizontally scalable
microservices. It is the **Tier 3 application layer** of the kcenon ecosystem.

See [`../FEATURES.md`](../FEATURES.md) for the complete capability matrix.

### Who is this for?

Game studios building multiplayer game servers in C++20 who want a
production-ready foundation with cloud-native deployment, observability,
and a clear extension model via plugins.

### Is it production-ready?

The project is **pre-1.0** (currently v0.1.0). Core features are stable, but
APIs may change between minor versions. See [`../VERSIONING.md`](../VERSIONING.md)
for the stability promise.

### What license is it under?

BSD 3-Clause License. Commercial use is permitted without royalty obligations.
See [`../LICENSE`](../LICENSE) and [`../LICENSE-THIRD-PARTY`](../LICENSE-THIRD-PARTY).

## Build & Setup

### Which compilers are supported?

GCC 11+, Clang 14+, Apple Clang 14+, MSVC 2022+. C++20 is required.
See [`../COMPATIBILITY.md`](../COMPATIBILITY.md).

### Conan or vcpkg?

Conan 2 is the primary package manager via the `conan-release` and `conan-debug`
presets. vcpkg is supported via the `vcpkg` preset. Both work; pick whichever
fits your existing toolchain.

### Can I build without a package manager?

Yes. Use the `default` CMake preset, which uses `FetchContent` to clone
kcenon dependencies from GitHub directly. Slower than Conan but requires no
external tools.

### How do I enable plugin hot reload?

Configure with `-DCGS_HOT_RELOAD=ON`. **This is development-only** —
production builds must leave it OFF for security and stability. See
[`../adr/ADR-003-plugin-hot-reload.md`](../adr/ADR-003-plugin-hot-reload.md).

## Architecture

### Why ECS instead of OOP hierarchies?

Performance, composition flexibility, and parallelism. ECS gives cache-friendly
data layout, O(1) component add/remove, and DAG-based parallel system execution.
See [`../adr/ADR-002-entity-component-system.md`](../adr/ADR-002-entity-component-system.md).

### Why exactly 5 microservices?

Each service represents a distinct scaling profile and failure domain
(Auth, Gateway, Game, Lobby, DBProxy). Splitting further would add
inter-service overhead without benefit. See
[`../adr/ADR-004-microservice-decomposition.md`](../adr/ADR-004-microservice-decomposition.md).

### Why depend on the kcenon ecosystem?

Each kcenon foundation system is independently versioned, benchmarked, and
stress-tested. Ecosystem improvements are inherited automatically without
re-implementation. See [`../ECOSYSTEM.md`](../ECOSYSTEM.md).

### What is the maximum entity count?

The framework targets 10,000 entities processed in <5 ms per tick (20 Hz).
Higher counts work but require profiling and possibly multiple GameServer
instances. The hard ID limit is effectively unlimited (64-bit version recycling).

## Plugins

### Can I write plugins in languages other than C++?

Currently no — plugins must export a C++ ABI (`extern "C" GamePlugin* cgs_create_plugin()`).
Bindings to scripting languages (Lua, Python) are out of scope for v0.x and may be
considered post-1.0.

### How does plugin state survive a hot reload?

Plugins implement `serialize`/`deserialize` hooks that snapshot state to a
flat byte buffer before unload, then restore it after the new version is
loaded. State is held in framework-managed memory during the swap.

### Can plugins talk to each other?

Yes, via the shared event bus. Plugins subscribe to typed events and publish
events that other plugins can react to. See
[`PLUGIN_DEVELOPMENT_GUIDE.md`](PLUGIN_DEVELOPMENT_GUIDE.md).

### Are plugins sandboxed?

No. Plugins run in the GameServer process and can access any system resource
the GameServer has. Production deployments must restrict the `plugins/`
directory to trusted operators.

## Microservices

### How do services communicate?

gRPC over TLS for production; UNIX domain sockets in local development.
Correlation IDs flow through every service hop for distributed tracing.

### Can I deploy services on different hosts?

Yes. The default deployment uses Kubernetes with each service as a separate
Deployment. Services discover each other via Kubernetes Services. See
[`DEPLOYMENT_GUIDE.md`](DEPLOYMENT_GUIDE.md).

### Can I run multiple GameServer instances?

Yes. Each GameServer instance handles a subset of map instances. The Lobby
Server distributes players across instances. See [`../ARCHITECTURE.md`](../ARCHITECTURE.md).

## Performance

### What is the world tick rate?

20 Hz (50 ms per tick). This is fixed by design — see
[`../ARCHITECTURE.md`](../ARCHITECTURE.md). Higher rates require profile-driven
optimization.

### How many concurrent users can it handle?

Target: 10,000 CCU per cluster. Validated by load tests. Higher CCU is possible
with horizontal scaling (more GameServer / GatewayServer replicas).

### Where can I see benchmarks?

[`../BENCHMARKS.md`](../BENCHMARKS.md) tracks measured performance.
[`../performance/BENCHMARKS_METHODOLOGY.md`](../performance/BENCHMARKS_METHODOLOGY.md)
explains how the numbers are produced.

## Database

### Which databases are supported?

PostgreSQL 14+ via the kcenon `database_system`. Other backends (SQLite,
MongoDB, Redis) are planned post-1.0.

### How do I run migrations?

The framework does not bundle a migration tool. Use any standard migration
tool (Flyway, Liquibase, sqitch) against the PostgreSQL backend.

## Security

### How do I report a vulnerability?

Email kcenon@naver.com or use [GitHub Security Advisories](https://github.com/kcenon/common_game_server/security/advisories/new).
See [`../SECURITY.md`](../SECURITY.md).

### Is the framework hardened?

The framework follows defense-in-depth: TLS 1.3 termination, JWT RS256
authentication, input validation at every boundary, prepared statements
for SQL, rate limiting at the gateway. See
[`../PRODUCTION_QUALITY.md#security-posture`](../PRODUCTION_QUALITY.md).

### Are dependencies scanned for CVEs?

Yes, via `grype` against a CycloneDX SBOM. See
[`../SOUP.md`](../SOUP.md) and [`../contributing/CI_CD_GUIDE.md`](../contributing/CI_CD_GUIDE.md).

## Documentation

### Why is there a `docs/archive/sdlc/` directory?

It preserves the original SDLC documents (PRD, SRS, SDS, etc.) from before
the project aligned with the kcenon ecosystem documentation template. The
content has been migrated to active docs, but the originals are kept for
reference. See [`../archive/README.md`](../archive/README.md).

### Why are some docs bilingual?

Core user-facing docs have Korean translations (`.kr.md`) to match the kcenon
ecosystem convention. Advanced and contributing docs are English-only.

### How do I write project documentation?

See [`../contributing/DOCUMENTATION_GUIDELINES.md`](../contributing/DOCUMENTATION_GUIDELINES.md).

## Contributing

### How do I contribute?

See [`../../CONTRIBUTING.md`](../../CONTRIBUTING.md). Open a PR with a clear
description and tests.

### Where do I report bugs?

[GitHub Issues](https://github.com/kcenon/common_game_server/issues). Include
reproduction steps, environment, and logs.

### Is there a community chat?

[GitHub Discussions](https://github.com/kcenon/common_game_server/discussions)
is the primary forum. There is no Discord or Slack at this time.

## Roadmap

### What's planned for v1.0?

API stabilization, increased test coverage, all CI workflows green on every
PR, reproducible benchmarks. See [`../ROADMAP.md`](../ROADMAP.md).

### When will multi-backend database support arrive?

Planned post-1.0, depending on kcenon `database_system` multi-backend support.
Track progress at [`../ROADMAP.md`](../ROADMAP.md).

## See Also

- [`../faq.dox`](../faq.dox) — Doxygen-rendered FAQ
- [`TROUBLESHOOTING.md`](TROUBLESHOOTING.md) — Common issues and fixes
- [`../GETTING_STARTED.md`](../GETTING_STARTED.md) — 5-10 min tutorial
- [`../README.md`](../README.md) — Documentation index
