# Deprecation Notices

This document tracks APIs and behaviors that are deprecated and scheduled for
removal in a future release.

## Policy

Per [`../VERSIONING.md`](../VERSIONING.md):

1. A symbol marked deprecated in a MINOR release is removed no earlier than the
   next MAJOR release.
2. Deprecated symbols are tagged with `[[deprecated("...")]]` in code and
   announced in the [GitHub Release notes](https://github.com/kcenon/common_game_server/releases).
3. Each deprecation entry includes:
   - The symbol or behavior
   - The version it was deprecated in
   - The planned removal version
   - The recommended replacement

## Active Deprecations

> **Status**: As of v0.1.0, no APIs are deprecated. This file is a placeholder
> that will be populated as the project evolves.

| Symbol / Behavior | Deprecated In | Removed In | Replacement |
|-------------------|--------------|------------|-------------|
| _(none)_ | — | — | — |

## Removed APIs

> **Status**: As of v0.1.0, no APIs have been removed. This file is a placeholder.

| Symbol / Behavior | Removed In | Replacement |
|-------------------|-----------|-------------|
| _(none)_ | — | — |

## How to Add an Entry

When deprecating an API:

1. Mark the symbol in code:
   ```cpp
   [[deprecated("Use foo() instead. Will be removed in v1.0.0.")]]
   void old_foo();
   ```
2. Add an entry to **Active Deprecations** in this file
3. Add an entry to [`../CHANGELOG.md`](../CHANGELOG.md) under `### Deprecated`
4. Update [`API_REFERENCE.md`](API_REFERENCE.md) to flag the deprecation
5. Cross-reference from [`TRACEABILITY.md`](TRACEABILITY.md)

## Migration Guides

When a deprecation is non-trivial, a migration guide is added under
[`guides/`](guides/). For example: `guides/MIGRATION_v1.md`.

## See Also

- [`../VERSIONING.md`](../VERSIONING.md) — Versioning policy and deprecation policy
- [`COMPATIBILITY.md`](COMPATIBILITY.md) — Platform and compiler support
- [`../CHANGELOG.md`](../CHANGELOG.md) — Release history with deprecation entries
