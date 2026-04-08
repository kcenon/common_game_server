# Documentation Guidelines

This document describes how to write documentation for `common_game_server`.
The project follows the kcenon ecosystem documentation conventions.

## Audience

When writing docs, identify your audience first:

| Audience | Examples | Voice |
|----------|----------|-------|
| New users | `GETTING_STARTED.md`, `README.md` | Welcoming, step-by-step |
| API consumers | `API_REFERENCE.md`, `API_QUICK_REFERENCE.md` | Precise, technical |
| Operators | `guides/DEPLOYMENT_GUIDE.md` | Action-oriented, runbook style |
| Plugin authors | `guides/PLUGIN_DEVELOPMENT_GUIDE.md` | Tutorial + reference hybrid |
| Maintainers | `adr/`, `advanced/`, `contributing/` | Decision-focused, in-depth |

## Document Types

### Core docs (bilingual)

These exist as `.md` + `.kr.md` pairs:

- `README.md`, `ARCHITECTURE.md`, `FEATURES.md`, `API_REFERENCE.md`,
  `BENCHMARKS.md`, `PROJECT_STRUCTURE.md`, `PRODUCTION_QUALITY.md`,
  `CHANGELOG.md`

When updating one, update both. The English version is the source of truth.

### Single-language docs (English only)

- All `guides/*`, all `advanced/*`, all `adr/*`, `ECOSYSTEM.md`,
  `COMPATIBILITY.md`, `DEPRECATION.md`, `TRACEABILITY.md`, `SOUP.md`

### Doxygen content (`.dox` files)

- `mainpage.dox` — Doxygen homepage
- `faq.dox`, `troubleshooting.dox`, `tutorial_*.dox`

These are consumed by Doxygen and rendered as HTML pages alongside the
auto-generated API reference.

## Style

### Headings

- Use sentence case: `## Getting started` (not `## Getting Started`)
- One H1 per file (`#`), then H2 (`##`) for top-level sections
- Avoid skipping levels (`##` directly to `####`)

### Code blocks

- Always specify a language hint: ` ```cpp `, ` ```bash `, ` ```yaml `
- Keep snippets focused; cut unrelated lines
- Use `// ...` to indicate elision

### Links

- Prefer relative paths: `[ARCHITECTURE.md](ARCHITECTURE.md)`
- Use repo-relative paths for cross-directory links: `[CONTRIBUTING.md](../CONTRIBUTING.md)`
- For external links, use `https://` (not bare URLs)
- For GitHub issues/PRs, use `#NNN` so they auto-link in GitHub UI

### Tables

- Use markdown tables for structured data
- Keep column headers short
- Right-align numeric columns when possible

### Tone

- Be direct and clear
- Avoid marketing language ("amazing", "blazing fast")
- Prefer present tense ("the scheduler runs systems") over future ("will run")
- Explain *why* in addition to *what*

## File Naming

| Type | Convention | Example |
|------|-----------|---------|
| Bilingual core doc | `UPPER_SNAKE_CASE.md` + `.kr.md` | `ARCHITECTURE.md`, `ARCHITECTURE.kr.md` |
| Single-language doc | `UPPER_SNAKE_CASE.md` | `ECOSYSTEM.md` |
| Guide | `UPPER_SNAKE_CASE.md` under `guides/` | `BUILD_GUIDE.md` |
| ADR | `ADR-NNN-hyphenated-slug.md` | `ADR-001-unified-game-server-architecture.md` |
| Template | `*_TEMPLATE.md` under `templates/` | `FEATURE_TEMPLATE.md` |
| Doxygen content | `*.dox` | `mainpage.dox` |

## ADR Template

ADRs use YAML frontmatter:

```yaml
---
doc_id: "CGS-ADR-NNN"
doc_title: "ADR-NNN: Decision Topic"
doc_version: "1.0.0"
doc_date: "YYYY-MM-DD"
doc_status: "Proposed|Accepted|Deprecated|Superseded"
project: "common_game_server"
category: "ADR"
---
```

Required sections:
1. **Context** — What problem are we solving?
2. **Decision** — What did we decide?
3. **Alternatives Considered** — What did we reject and why?
4. **Consequences** — Positive and negative outcomes

See [`templates/ADR_TEMPLATE.md`](templates/ADR_TEMPLATE.md) and
[`../adr/`](../adr/) for examples.

## Bilingual Translation

When creating a `.kr.md` companion:

1. Mirror the structure exactly (same headings, same tables)
2. Translate prose, not code
3. Translate table headers; keep code identifiers in English
4. Add a header link back to the English version
5. Update both files in the same commit when content changes

Example header pair:

```markdown
# Architecture

> **English** · [한국어](ARCHITECTURE.kr.md)
```

```markdown
# 아키텍처

> [English](ARCHITECTURE.md) · **한국어**
```

## Doxygen Comments

For C++ source code, use Doxygen-style comments:

```cpp
/**
 * @brief Brief description (one line)
 * @param name Description of name
 * @return Description of return value
 * @thread_safety Thread-safe / Not thread-safe
 * @performance O(1) / O(n) / etc.
 */
auto my_function(std::string_view name) -> Result<int>;
```

The `@thread_safety` and `@performance` aliases are defined in the `Doxyfile`.

## Linking from Code Comments to Markdown Docs

To reference markdown docs from Doxygen comments:

```cpp
/**
 * @brief Plugin lifecycle interface.
 *
 * See guides/PLUGIN_DEVELOPMENT_GUIDE.md for the complete authoring guide.
 * See ADR-003 for the hot reload design rationale.
 */
class GamePlugin { /* ... */ };
```

## Updating Documentation

When you make a code change:

1. Update the relevant `API_REFERENCE.md` section
2. If the change is user-visible, add an entry to `../CHANGELOG.md`
3. If the change deprecates an API, add an entry to `DEPRECATION.md`
4. If the change affects architecture, update `ARCHITECTURE.md`
5. Update `TRACEABILITY.md` if cross-references are affected

CI verifies that critical files exist and that markdown links are not broken.

## Linting

Markdown linting (planned):

```bash
markdownlint docs/
```

Link checking (planned):

```bash
markdown-link-check docs/**/*.md
```

## See Also

- [`CONTRIBUTING.md`](CONTRIBUTING.md) — Contribution workflow
- [`CI_CD_GUIDE.md`](CI_CD_GUIDE.md) — How CI processes docs
- [`templates/`](templates/) — Markdown templates
