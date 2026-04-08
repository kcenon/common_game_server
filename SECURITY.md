# Security Policy

## Supported Versions

| Version | Supported          |
|---------|--------------------|
| Latest  | :white_check_mark: |
| < Latest | :x:               |

Only the latest release receives security updates. Users are encouraged to
upgrade to the most recent version.

## Reporting a Vulnerability

If you discover a security vulnerability in Common Game Server, please report
it responsibly through one of the following channels:

- **Email**: [kcenon@naver.com](mailto:kcenon@naver.com)
- **GitHub Security Advisories**: [Report here](https://github.com/kcenon/common_game_server/security/advisories/new)

**Please do NOT report security vulnerabilities through public GitHub issues.**

### What to Include

- Description of the vulnerability
- Steps to reproduce the issue
- Potential impact assessment
- Suggested fix (if any)

### Response Timeline

| Stage | Timeframe |
|-------|-----------|
| Acknowledgment | Within 7 days |
| Initial assessment | Within 14 days |
| Fix and release | Depends on severity |

### After Reporting

1. You will receive an acknowledgment within 7 days
2. The maintainers will investigate and provide an initial assessment
3. A fix will be developed and tested
4. A security advisory will be published with the fix release
5. Credit will be given to the reporter (unless anonymity is requested)

## Security Best Practices

When using Common Game Server in your projects:

- Always use the latest stable release
- Monitor the [GitHub Security Advisories](https://github.com/kcenon/common_game_server/security/advisories) page for updates
- Follow the dependency update recommendations in [DEPENDENCY_MATRIX.md](DEPENDENCY_MATRIX.md)
- Review the [Security Best Practices guide](docs/guides/SECURITY_BEST_PRACTICES.md) (when available)

## Scope

This policy covers:

- The core `cgs_core` library and all first-party sources under `src/` and `include/`
- Built-in services (Auth, Gateway, Game, Lobby, DBProxy)
- First-party plugins shipped in this repository
- Deployment manifests (`deploy/`)

It does **not** cover:

- Third-party dependencies declared via Conan — report to upstream
- User-authored plugins — report to the plugin author
- Modified forks — report to the fork maintainer
