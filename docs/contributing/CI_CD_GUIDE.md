# CI/CD Guide

This document explains the continuous integration and deployment workflows
for `common_game_server`.

## Workflows Overview

| Workflow | File | Trigger | Purpose |
|----------|------|---------|---------|
| CI | `.github/workflows/ci.yml` | push, PR | Lint + build + test (3 configs) |
| Code Coverage | `.github/workflows/coverage.yml` | push, PR | lcov coverage report |
| Generate-Documentation | `.github/workflows/build-Doxygen.yaml` | push, PR | Doxygen HTML generation; deploys to `gh-pages` on main push (calls `kcenon/common_system/.github/workflows/doxygen.yml`) |
| Benchmarks | `.github/workflows/benchmarks.yml` | manual / scheduled | Performance benchmark suite |
| Load Test | `.github/workflows/load-test.yml` | manual | CCU validation scripts |
| Chaos Tests | `.github/workflows/chaos-tests.yml` | manual | Fault injection & resilience |

## CI Workflow (Detail)

```
lint (clang-format 21.1.8)
  └─> build & test (ubuntu-24.04 Debug)
  └─> build & test (ubuntu-24.04 Release)
  └─> build & test (macos-14 Release)
```

### Lint stage

Runs `clang-format --dry-run --Werror` against all `*.hpp` and `*.cpp` files
under `include/cgs/` and `src/`. Fails the build on any formatting violation.

To check locally:

```bash
find include/cgs src -name '*.hpp' -o -name '*.cpp' | \
    xargs clang-format --dry-run --Werror
```

To auto-fix:

```bash
find include/cgs src -name '*.hpp' -o -name '*.cpp' | \
    xargs clang-format -i
```

### Build & test stage

Runs the following per configuration:

```bash
conan install . --output-folder=build --build=missing -s build_type=$BUILD_TYPE
cmake --preset conan-$build_type
cmake --build --preset conan-$build_type -j$(nproc)
ctest --preset conan-$build_type --output-on-failure
```

Build configurations:
- `ubuntu-24.04` × `Debug` (with sanitizers off; coverage on for the Coverage workflow)
- `ubuntu-24.04` × `Release`
- `macos-14` × `Release` (Apple Silicon)

## Code Coverage Workflow

Uses `lcov` to generate coverage reports. Configured via [`../codecov.yml`](../codecov.yml).

Targets (from `codecov.yml`):
- Project: 40% minimum (currently informational, doesn't block PRs)
- Patch: 60% minimum

To run coverage locally:

```bash
cmake --preset conan-debug -DCGS_ENABLE_COVERAGE=ON
cmake --build --preset conan-debug
ctest --preset conan-debug
lcov --capture --directory build/Debug --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' '*/_deps/*' --output-file coverage.info
lcov --list coverage.info
```

## Docs Workflow

Runs `doxygen Doxyfile` and uploads the generated HTML as a GitHub Pages
artifact. Doxygen 1.12.0 is pinned for reproducibility.

Strict mode: `WARN_NO_PARAMDOC = YES` and `WARN_AS_ERROR` are set, so any
missing parameter docs cause CI failure.

To verify locally:

```bash
doxygen -x Doxyfile > /dev/null  # validate config
doxygen Doxyfile                  # generate docs
open documents/html/index.html    # macOS
xdg-open documents/html/index.html  # Linux
```

## Benchmarks Workflow

Runs the Google Benchmark suite under `tests/benchmark/`. Results are
stored as workflow artifacts and compared against the previous run for
regression detection (10% threshold).

Manual trigger:

```bash
gh workflow run benchmarks.yml -R kcenon/common_game_server
```

## Load Test Workflow

Runs scripts under `tests/load/` against a Docker Compose stack. Validates
the 10K CCU target.

## Chaos Test Workflow

Runs fault injection tests under `tests/chaos/` to verify circuit breakers,
graceful shutdown, and database failover behavior.

## CI Status Tracking

### Required checks

For a PR to merge, **all** of the following must be passing:

- `lint`
- `build & test (ubuntu-24.04 Debug)`
- `build & test (ubuntu-24.04 Release)`
- `build & test (macos-14 Release)`
- `coverage`
- `docs`

### Optional checks

These run but don't block merges:

- `benchmarks` (manual)
- `load-test` (manual)
- `chaos-tests` (manual)

## How to Wait for CI After Pushing

Use `gh pr checks <PR_NUMBER>` to verify ALL individual check statuses, not
just `gh run list` (which shows workflow-level status that can hide failing
sub-jobs):

```bash
gh pr checks 123
```

Poll at 30-second intervals; max 10 minutes per run.

## Troubleshooting

### "clang-format mismatch"

Local clang-format version differs from CI (21.1.8). Install matching version
or run `clang-format -i` from a Docker container.

### "Doxygen warning: parameter ... has no description"

`WARN_NO_PARAMDOC` is enabled. Add `@param name Description` for every
function parameter, or remove unused parameter documentation.

### "Coverage report failed to upload"

Codecov may be rate-limited. The workflow is `informational: true`, so this
does not block PRs.

### "Test passed locally but fails in CI"

Common causes:
- Test depends on local environment (filesystem path, env var, locale)
- Race condition that only manifests under CI's load
- Missing test fixture file not committed to git

## See Also

- [`../../CONTRIBUTING.md`](../../CONTRIBUTING.md) — Contribution workflow
- [`../guides/TROUBLESHOOTING.md`](../guides/TROUBLESHOOTING.md) — Build troubleshooting
- [`DOCUMENTATION_GUIDELINES.md`](DOCUMENTATION_GUIDELINES.md) — How docs are built
