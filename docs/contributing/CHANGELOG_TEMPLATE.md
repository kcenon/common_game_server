# Changelog Entry Template

Use this template when adding entries to [`../../CHANGELOG.md`](../../CHANGELOG.md)
or [`../CHANGELOG.md`](../CHANGELOG.md).

## Format

The project follows [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/).
Entries are added under the `## [Unreleased]` section until a release is cut.

## Section Guide

| Section | When to use |
|---------|-------------|
| **Added** | New features, new files, new dependencies |
| **Changed** | Behavior changes that don't break existing usage |
| **Deprecated** | Features marked deprecated (still functional) |
| **Removed** | Features that no longer exist |
| **Fixed** | Bug fixes |
| **Security** | Vulnerability fixes |

## Entry Format

```markdown
- One-line description of the change ([#PR_NUMBER](https://github.com/kcenon/common_game_server/issues/PR_NUMBER))
```

Rules:
- Start with a verb in past tense ("Added", "Fixed", "Removed")
- Wait, the *section* uses the verb. Each entry uses present-tense imperative or descriptive form.
- One sentence per entry; if more is needed, link to a PR or issue
- Always include a PR/issue link
- Group multiple entries by section, not chronologically

## Examples

### Good

```markdown
## [Unreleased]

### Added
- Support for Windows MSVC 2022 builds in CI ([#234](https://github.com/kcenon/common_game_server/issues/234))
- `tutorial_plugin.dox` walkthrough for plugin authoring ([#235](https://github.com/kcenon/common_game_server/issues/235))

### Changed
- `EntityManager::create()` now returns `entity_id` directly instead of `Result<entity_id>` ([#240](https://github.com/kcenon/common_game_server/issues/240))

### Fixed
- Race condition in `PluginManager::load()` when called concurrently ([#241](https://github.com/kcenon/common_game_server/issues/241))

### Deprecated
- `cgs::core::Result::unwrap_or_throw()` will be removed in v2.0 — use `value()` with explicit check ([#243](https://github.com/kcenon/common_game_server/issues/243))
```

### Bad

```markdown
## [Unreleased]

- Various improvements    ← too vague, no link
- Fixed a bug             ← which bug? no link
- Added stuff             ← what stuff?
```

## When to Add an Entry

Add a CHANGELOG entry for any user-visible change:

| Change type | Add entry? |
|-------------|-----------|
| New public API | Yes (Added) |
| New CMake option | Yes (Added) |
| API behavior change | Yes (Changed) |
| Bug fix in public API | Yes (Fixed) |
| Internal refactor | No (unless behavior changes) |
| Test added/changed | No |
| CI workflow added | Yes (Added → "Infrastructure" sub-section if needed) |
| Documentation only | Yes if user-facing (Added/Changed under docs) |
| Dependency version bump | Yes (Changed) |
| Deprecation notice | Yes (Deprecated) + entry in `DEPRECATION.md` |

## Updating on Release

When cutting a release:

1. Move all entries from `## [Unreleased]` to a new `## [X.Y.Z] - YYYY-MM-DD` section
2. Add a fresh empty `## [Unreleased]` section above
3. Verify all entries have PR links
4. Commit with message `chore(release): bump version to X.Y.Z`

See [`../../VERSIONING.md`](../../VERSIONING.md) for the full release process.

## See Also

- [`../../CHANGELOG.md`](../../CHANGELOG.md) — Project changelog (root)
- [`../CHANGELOG.md`](../CHANGELOG.md) — Mirror in docs/
- [`../DEPRECATION.md`](../DEPRECATION.md) — Deprecation tracking
- [`../../VERSIONING.md`](../../VERSIONING.md) — Versioning policy
