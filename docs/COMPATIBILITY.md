# Compatibility

This document tracks the supported C++ standards, compilers, platforms, and
operating system versions for `common_game_server`.

## C++ Standard

**Minimum**: C++20

`common_game_server` makes extensive use of C++20 features:
- Concepts (`std::same_as`, custom concepts in queries)
- Three-way comparison (`<=>`)
- `std::span`, `std::format` (where supported)
- `consteval` and improved `constexpr`
- Coroutines (in foundation adapters)

C++23 features are **not** required and are not used until all supported
compilers ship them.

## Compilers

| Compiler | Minimum Version | Tested in CI |
|----------|----------------|--------------|
| GCC | 11.4 | Yes (ubuntu-24.04) |
| Clang | 14 | Yes (ubuntu-24.04) |
| Apple Clang | 14 | Yes (macos-14) |
| MSVC | 2022 (19.30+) | Best-effort, no CI |

GCC 10 and Clang 13 are **not** supported because they lack required C++20
concept support.

## Platforms

| Platform | Support Level | CI Coverage |
|----------|---------------|-------------|
| Ubuntu 22.04+ (x86_64) | First-class | Debug + Release |
| Ubuntu 24.04 (x86_64) | First-class | Debug + Release |
| Debian 12+ (x86_64) | Best-effort | None |
| macOS 14+ (Apple Silicon) | First-class | Release |
| macOS 14+ (Intel) | Best-effort | None |
| Windows 10/11 (MSVC 2022) | Best-effort | None |
| Linux (aarch64) | Best-effort | None |
| FreeBSD / OpenBSD | Unsupported | None |

**First-class** = tested on every PR; bugs are fixed before merging.
**Best-effort** = should work; user-reported issues are investigated.
**Unsupported** = not tested; PRs accepted but no maintenance commitment.

## CMake

**Minimum**: CMake 3.20

The project uses CMake presets ([`../CMakePresets.json`](../CMakePresets.json)),
which require CMake 3.20+.

## Database

**Minimum**: PostgreSQL 14

The DBProxy service connects to PostgreSQL using the `libpq` interface (via
the kcenon `database_system`). PostgreSQL 14+ features used:
- Generated columns
- `STORED` generated columns in queries
- Partitioned tables for time-series data

Earlier versions of PostgreSQL **may** work but are not tested.

## Container Runtime

| Runtime | Tested |
|---------|--------|
| Docker 24+ | Yes |
| Podman 4+ | Best-effort |
| containerd 1.7+ | Best-effort |

## Kubernetes

**Minimum**: Kubernetes 1.27 (uses HPA v2, PDB, StatefulSet)

Newer Kubernetes versions (1.28, 1.29, 1.30) are expected to work without
manifest changes.

## ABI Stability

`common_game_server` is **pre-1.0** and makes no ABI stability guarantees.
After v1.0.0, ABI changes will require a MAJOR version bump per
[`../VERSIONING.md`](../VERSIONING.md).

## Plugin ABI

Plugin shared libraries must be compiled with:
- The same C++ standard (C++20)
- The same compiler family (GCC plugins for GCC server, Clang for Clang)
- A compatible standard library (libstdc++ or libc++; not mixed)
- The same `cgs::plugin::PLUGIN_ABI_VERSION` macro value

Loading a plugin built against a different `PLUGIN_ABI_VERSION` will fail
with `error_code::plugin_abi_mismatch`.

## Network Protocol Compatibility

The wire protocol carries a version byte. The gateway negotiates the highest
mutually-supported version. See
[`advanced/PROTOCOL_SPECIFICATION.md`](advanced/PROTOCOL_SPECIFICATION.md).

## Cross-Compilation

Cross-compilation is **not** officially supported. Contributors are welcome to
submit cross-compilation profiles, but CI does not exercise them.

## See Also

- [`../VERSIONING.md`](../VERSIONING.md) — Versioning policy
- [`PRODUCTION_QUALITY.md`](PRODUCTION_QUALITY.md) — SLOs and quality gates
- [`DEPRECATION.md`](DEPRECATION.md) — Deprecated APIs
