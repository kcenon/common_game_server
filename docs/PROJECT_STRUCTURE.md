# Project Structure

> **English** · [한국어](PROJECT_STRUCTURE.kr.md)

```
common_game_server/
├── README.md / README.kr.md      Project overview (bilingual)
├── CLAUDE.md                     Agent-readable architecture summary
├── CHANGELOG.md                  Release history (Keep a Changelog)
├── CODE_OF_CONDUCT.md            Community standards
├── CONTRIBUTING.md               Contribution workflow
├── SECURITY.md                   Vulnerability reporting policy
├── LICENSE                       BSD 3-Clause license
├── LICENSE-THIRD-PARTY           Dependency licenses
├── NOTICES                       SOUP version notices
├── DEPENDENCY_MATRIX.md          Upstream/downstream version table
├── VERSION                       Semantic version (single line)
├── VERSIONING.md                 SemVer policy
├── CMakeLists.txt                Root CMake configuration
├── CMakePresets.json             Standard build presets
├── codecov.yml                   Coverage configuration
├── conanfile.py                  Conan recipe
├── Doxyfile                      Doxygen configuration
├── build.sh                      Convenience build script
├── .clang-format                 Code style (Google-based, 4-space indent)
├── .clang-tidy                   Static analysis rules
│
├── include/cgs/                  Public headers (installed)
│   ├── core/                       Result, error codes, types
│   ├── ecs/                        Entity-Component System
│   ├── foundation/                 Foundation adapter interfaces
│   ├── game/                       Game logic types
│   ├── plugin/                     Plugin interfaces
│   ├── service/                    Service types and configs
│   ├── cgs.hpp                     Umbrella header
│   └── version.hpp                 Compile-time version macros
│
├── src/                          Implementation (compiled into layer libs)
│   ├── ecs/                        ECS core
│   ├── foundation/                 7 foundation adapters
│   ├── game/                       6 game logic systems
│   ├── plugins/                    Plugin manager + sample plugins
│   │   ├── manager/
│   │   └── mmorpg/                 Sample MMORPG plugin
│   └── services/                   5 microservices + shared runner
│       ├── shared/                 service_runner template
│       ├── auth/
│       ├── gateway/
│       ├── game/
│       ├── lobby/
│       └── dbproxy/
│
├── tests/                        Test suites
│   ├── unit/                       Unit tests (Google Test)
│   ├── integration/                Integration tests
│   ├── benchmark/                  Performance benchmarks (Google Benchmark)
│   ├── load/                       Load testing scripts
│   └── chaos/                      Chaos / fault injection tests
│
├── deploy/                       Deployment configuration
│   ├── docker/                     Dockerfile per service
│   ├── k8s/                        Kubernetes manifests (HPA, PDB, StatefulSet)
│   ├── monitoring/                 Prometheus + Grafana dashboards
│   ├── config/                     Service config templates
│   └── docker-compose.yml          Local full-stack development
│
├── config/                       Default service configuration
├── cmake/                        Helper CMake modules (kcenon_deps.cmake, ...)
├── scripts/                      Utility scripts
│
└── docs/                         Documentation (kcenon ecosystem template)
    ├── README.md                   Index
    ├── GETTING_STARTED.md          5-10 min tutorial
    ├── API_QUICK_REFERENCE.md      1-page cheat sheet
    ├── API_REFERENCE.md / .kr.md   Full API docs (bilingual)
    ├── ARCHITECTURE.md / .kr.md    System design (bilingual)
    ├── FEATURES.md / .kr.md        Feature matrix (bilingual)
    ├── BENCHMARKS.md / .kr.md      Performance metrics (bilingual)
    ├── PRODUCTION_QUALITY.md / .kr.md  SLA & readiness (bilingual)
    ├── PROJECT_STRUCTURE.md / .kr.md   This file (bilingual)
    ├── CHANGELOG.md / .kr.md       Release history mirror
    ├── ECOSYSTEM.md                kcenon ecosystem position
    ├── COMPATIBILITY.md            C++/platform support
    ├── DEPRECATION.md              Deprecated APIs
    ├── TRACEABILITY.md             Cross-doc references
    ├── SOUP.md                     Third-party BOM
    ├── ROADMAP.md                  Implementation milestones
    │
    ├── adr/                        Architecture Decision Records
    │   ├── ADR-001-unified-game-server-architecture.md
    │   ├── ADR-002-entity-component-system.md
    │   ├── ADR-003-plugin-hot-reload.md
    │   └── ADR-004-microservice-decomposition.md
    │
    ├── advanced/                   Deep-dive references
    │   ├── ECS_DEEP_DIVE.md
    │   ├── FOUNDATION_ADAPTERS.md
    │   ├── PROTOCOL_SPECIFICATION.md
    │   └── DATABASE_SCHEMA.md
    │
    ├── guides/                     How-to & operational
    │   ├── BUILD_GUIDE.md
    │   ├── DEPLOYMENT_GUIDE.md
    │   ├── CONFIGURATION_GUIDE.md
    │   ├── TESTING_GUIDE.md
    │   ├── PLUGIN_DEVELOPMENT_GUIDE.md
    │   ├── INTEGRATION_GUIDE.md
    │   ├── TROUBLESHOOTING.md
    │   └── FAQ.md
    │
    ├── contributing/               Contribution process
    │   ├── CODING_STANDARDS.md
    │   ├── DOCUMENTATION_GUIDELINES.md
    │   ├── CI_CD_GUIDE.md
    │   ├── CHANGELOG_TEMPLATE.md
    │   └── templates/
    │
    ├── performance/                Performance analysis
    │   └── BENCHMARKS_METHODOLOGY.md
    │
    ├── doxygen-awesome-css/        Doxygen theme (vendored)
    ├── custom.css                  Doxygen branding overrides
    ├── header.html                 Doxygen HTML template
    ├── mainpage.dox                Doxygen homepage
    ├── faq.dox                     Doxygen FAQ
    ├── troubleshooting.dox         Doxygen troubleshooting
    ├── tutorial_*.dox              Doxygen tutorials
    │
    └── archive/                    Historical preservation
        ├── README.md
        └── sdlc/                   Legacy SDLC docs (PRD/SRS/SDS/...)
```

## Key Conventions

| Convention | Where it applies |
|------------|------------------|
| `cgs::` namespace prefix | All public APIs |
| `snake_case` for files, classes, functions | Source code |
| `UPPER_SNAKE_CASE` for constants and macros | Source code |
| `CGS_*` macro prefix | Build options, version macros |
| `*.hpp` extension | All C++ headers |
| `*.cpp` extension | All C++ implementation files |
| `*.dox` extension | Doxygen content files |
| `*.kr.md` suffix | Korean translations of bilingual core docs |

## Build Outputs

After running `cmake --build`, the following appear under `build/`:

```
build/
├── lib/                          Static libraries
│   ├── libcgs_core.a
│   ├── libcgs_foundation.a
│   ├── libcgs_ecs.a
│   ├── libcgs_game.a
│   ├── libcgs_plugin.a
│   └── libcgs_services.a
├── bin/                          Executables (when CGS_BUILD_SERVICES=ON)
│   ├── auth_server
│   ├── gateway_server
│   ├── game_server
│   ├── lobby_server
│   └── dbproxy
└── plugins/                      Sample plugin shared libraries
    └── libmmorpg_plugin.so
```

Doxygen output (after `doxygen Doxyfile`):

```
documents/
└── html/
    └── index.html               Generated API documentation
```

## See Also

- [`ARCHITECTURE.md`](ARCHITECTURE.md) — How the layers fit together
- [`FEATURES.md`](FEATURES.md) — What each component provides
- [`../CONTRIBUTING.md`](../CONTRIBUTING.md) — How to contribute changes
