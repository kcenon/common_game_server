# Versioning Policy

This document defines the semantic versioning policy and release process for
`common_game_server`. It inherits from the kcenon ecosystem versioning policy
maintained in [`common_system/VERSIONING.md`](https://github.com/kcenon/common_system/blob/main/VERSIONING.md)
and adds game-server-specific notes.

> **Inherits from**: kcenon ecosystem versioning policy (Tier 0: common_system)
> **Applies to**: common_game_server only

## Version Format

`common_game_server` uses [Semantic Versioning 2.0.0](https://semver.org/):

```
MAJOR.MINOR.PATCH
```

| Component | Increment when |
|-----------|----------------|
| **MAJOR** | Breaking changes to public API, network protocol, or plugin ABI |
| **MINOR** | New features added in a backward-compatible manner |
| **PATCH** | Backward-compatible bug fixes |

Tag format: `v{MAJOR}.{MINOR}.{PATCH}` (e.g., `v0.1.0`, `v1.0.0`)

Pre-release versions append a hyphen suffix: `v1.0.0-rc.1`, `v1.0.0-beta.2`

## Breaking Change Definition

A **MAJOR** version bump is required when any of the following occur:

- **Public API removal or rename** — Removing or renaming any public header,
  function, class, or type in `include/cgs/`
- **Network protocol change** — Incrementing the wire protocol version or
  changing any on-the-wire message format without a compatibility shim
- **Plugin ABI change** — Changing the `GamePlugin` interface or lifecycle
  callback signatures
- **Database schema change** — Migrations that require manual intervention
- **Service contract change** — Removing or renaming Auth/Gateway/Game/Lobby/
  DBProxy endpoints without deprecation
- **CMake interface change** — Renaming `cgs::cgs_core` or similar downstream-
  visible targets
- **C++ standard bump** — Increasing the minimum required C++ standard beyond C++20

Non-breaking changes that do **not** require a MAJOR bump:

- Adding new components, systems, or plugins
- Adding new service endpoints with backward-compatible request/response shapes
- Adding new CMake options with backward-compatible defaults
- Internal refactoring with no public interface change
- Deprecating (but not removing) a symbol
- Database migrations that are automatically applied and reversible
- Test, documentation, or CI changes

## Pre-1.0 Stability Promise

While `MAJOR == 0`, breaking changes may occur in **MINOR** releases. This
reflects the project's active development phase. Consumers pinning to a
specific `v0.x.y` tag are protected from unannounced breakage.

Once `v1.0.0` is tagged, full SemVer guarantees apply.

## Release Process

### Step 1 — Update version numbers

All three locations must agree before tagging:

| File | Location |
|------|----------|
| `VERSION` | Single-line `X.Y.Z` |
| `CMakeLists.txt` | `project(... VERSION X.Y.Z ...)` |
| `conanfile.py` | `version = "X.Y.Z"` |
| Git tag | `vX.Y.Z` |

### Step 2 — Update CHANGELOG.md

Move entries from `[Unreleased]` to a new `[X.Y.Z] - YYYY-MM-DD` section in
`CHANGELOG.md` (root) and `docs/CHANGELOG.md`.

### Step 3 — Commit and push version bump

```bash
git add VERSION CMakeLists.txt conanfile.py CHANGELOG.md docs/CHANGELOG.md
git commit -m "chore(release): bump version to X.Y.Z"
git push origin main
```

### Step 4 — Create and push the tag

```bash
git tag vX.Y.Z
git push origin vX.Y.Z
```

### Step 5 — Verify the release

Confirm the GitHub Release is published at:
`https://github.com/kcenon/common_game_server/releases/tag/vX.Y.Z`

## Changelog Generation

Release notes should follow the [Keep a Changelog](https://keepachangelog.com/)
format, with sections: `Added`, `Changed`, `Fixed`, `Deprecated`, `Removed`,
`Security`.

All entries should reference the GitHub issue or PR number: `(#123)`.

## Ecosystem Compatibility

`common_game_server` depends on pinned tagged versions of the kcenon
foundation systems. See [`DEPENDENCY_MATRIX.md`](DEPENDENCY_MATRIX.md) for the
current required versions.

## Deprecation Policy

Before removing a public symbol:

1. Mark it `[[deprecated("Use Foo instead")]]` in a MINOR release
2. Announce deprecation in the GitHub Release notes and [`docs/DEPRECATION.md`](docs/DEPRECATION.md)
3. Remove it in the next MAJOR release (not before)

This gives consumers at least one MINOR version cycle to migrate.

---

*Version: 1.0.0 | Established: 2026-04-08 | Owned by: common_game_server*
