# Software of Unknown Pedigree (SOUP)

This document is the **Bill of Materials** for `common_game_server`. It
enumerates every third-party software component that is built into or
required by the project, with version, license, source, and risk assessment.

> **Audience**: Compliance reviewers, security auditors, downstream consumers.

## Why "SOUP"?

The term "Software of Unknown Pedigree" originates from medical device
safety standards (IEC 62304) and refers to any third-party software that
is integrated into a system. Tracking SOUP is essential for:

- License compliance
- CVE / vulnerability tracking
- Reproducible builds
- Supply-chain security

## Direct Runtime Dependencies

| Package | Version | License | Source | Risk |
|---------|---------|---------|--------|------|
| yaml-cpp | 0.8.0 | MIT | https://github.com/jbeder/yaml-cpp | Low |

## Direct Build & Test Dependencies

| Package | Version | License | Source | Risk |
|---------|---------|---------|--------|------|
| googletest (gtest) | 1.15.0 | BSD-3-Clause | https://github.com/google/googletest | Low |
| google-benchmark | 1.9.1 | Apache-2.0 | https://github.com/google/benchmark | Low |

## kcenon Ecosystem Dependencies

These are **first-party** within the kcenon ecosystem but third-party from
the perspective of `common_game_server`:

| Library | Pinned Version | License | Source | Risk |
|---------|---------------|---------|--------|------|
| common_system | v0.2.0+ | BSD-3-Clause | https://github.com/kcenon/common_system | Low |
| thread_system | v0.2.0+ | BSD-3-Clause | https://github.com/kcenon/thread_system | Low |
| logger_system | v0.2.0+ | BSD-3-Clause | https://github.com/kcenon/logger_system | Low |
| network_system | v0.2.0+ | BSD-3-Clause | https://github.com/kcenon/network_system | Low |
| database_system | v0.2.0+ | BSD-3-Clause | https://github.com/kcenon/database_system | Low |
| monitoring_system | v0.2.0+ | BSD-3-Clause | https://github.com/kcenon/monitoring_system | Low |
| container_system | v0.2.0+ | BSD-3-Clause | https://github.com/kcenon/container_system | Low |

## Indirect (Transitive) Dependencies

The following are pulled in by direct dependencies. They are listed for
license and CVE tracking only — `common_game_server` does not consume their
APIs directly.

| Package | Pulled in by | License | Notes |
|---------|-------------|---------|-------|
| libpq | database_system | PostgreSQL | Linked when PostgreSQL backend is built |
| asio | network_system | Boost Software License 1.0 | Standalone (not Boost.Asio) |
| fmt | logger_system, common_system | MIT | std::format compatibility shim |

The complete transitive license tree is available in the SBOM
(see [CVE Scanning](#cve-scanning) below).

## Risk Assessment Criteria

| Risk | Criteria |
|------|----------|
| **Low** | Mature project, active maintainer, clear license, no known critical CVEs |
| **Medium** | Smaller maintainer base or older version, but no security concerns |
| **High** | Unmaintained, unclear license, or known unpatched CVEs |

## CVE Scanning

CVE scanning is performed against the SBOM using the `grype` tool. See
[`contributing/CI_CD_GUIDE.md`](contributing/CI_CD_GUIDE.md) for the workflow
configuration.

To scan locally:

```bash
# Generate CycloneDX SBOM
syft scan dir:. -o cyclonedx-json > sbom.json

# Run grype against the SBOM
grype sbom:sbom.json --fail-on critical
```

## License Compliance

All transitive licenses are compatible with the `common_game_server`
BSD-3-Clause license. License notices are preserved per the obligations of
each upstream license:

- **MIT / BSD-3-Clause** — Notice preserved in [`../LICENSE-THIRD-PARTY`](../LICENSE-THIRD-PARTY)
- **Apache-2.0** — `NOTICE` file preserved in [`../NOTICES`](../NOTICES)
- **Boost Software License 1.0** — Notice preserved in [`../LICENSE-THIRD-PARTY`](../LICENSE-THIRD-PARTY)
- **PostgreSQL License** — Notice preserved in [`../LICENSE-THIRD-PARTY`](../LICENSE-THIRD-PARTY)

## Update Policy

When a SOUP entry needs updating:

1. Verify the new version is compatible (build + tests pass)
2. Update the version in this document
3. Update [`../LICENSE-THIRD-PARTY`](../LICENSE-THIRD-PARTY) and [`../NOTICES`](../NOTICES) if needed
4. Update [`../DEPENDENCY_MATRIX.md`](../DEPENDENCY_MATRIX.md) for kcenon deps
5. Add an entry to [`../CHANGELOG.md`](../CHANGELOG.md) under `### Changed`
6. Run a CVE scan against the new version

## See Also

- [`../LICENSE-THIRD-PARTY`](../LICENSE-THIRD-PARTY) — Full license enumeration
- [`../NOTICES`](../NOTICES) — Apache-2.0 notice file
- [`../DEPENDENCY_MATRIX.md`](../DEPENDENCY_MATRIX.md) — kcenon ecosystem version compatibility
- [`COMPATIBILITY.md`](COMPATIBILITY.md) — Platform and compiler support matrix
- [`../SECURITY.md`](../SECURITY.md) — Vulnerability reporting

---

*SOUP version: aligned with `common_game_server` v0.1.0 — last reviewed 2026-04-08.*
