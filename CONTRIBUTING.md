# Contributing to Common Game Server

Thank you for considering contributing to Common Game Server! This document
provides guidelines and instructions for contributors.

## Table of Contents

- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Code Standards](#code-standards)
- [Testing](#testing)
- [Pull Requests](#pull-requests)

## Getting Started

### Prerequisites

- C++20 compatible compiler (GCC 11+, Clang 14+, MSVC 2022+, Apple Clang 14+)
- CMake 3.20 or higher
- Conan 2 package manager
- Git
- clang-format 21+ (for local formatting checks)

### Building from Source

```bash
git clone https://github.com/kcenon/common_game_server.git
cd common_game_server

# Install dependencies via Conan
conan install . --output-folder=build --build=missing -s build_type=Release

# Configure and build using CMake presets
cmake --preset conan-release
cmake --build --preset conan-release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

### Running Tests

```bash
ctest --preset conan-release --output-on-failure
```

See [`docs/guides/BUILD_GUIDE.md`](docs/guides/BUILD_GUIDE.md) for detailed
build instructions and [`docs/guides/TESTING_GUIDE.md`](docs/guides/TESTING_GUIDE.md)
for the full testing strategy.

## Development Workflow

1. Fork the repository
2. Create a feature branch (`git checkout -b feat/issue-N-description`)
3. Make your changes
4. Format your code: `find include/cgs src -name '*.hpp' -o -name '*.cpp' | xargs clang-format -i`
5. Run tests and ensure they pass
6. Commit your changes (see commit message guidelines below)
7. Push to your fork (`git push origin feat/issue-N-description`)
8. Open a Pull Request

### Commit Message Guidelines

Follow the [Conventional Commits](https://www.conventionalcommits.org/) format:

```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `refactor`: Code refactoring
- `test`: Adding or modifying tests
- `perf`: Performance improvement
- `chore`: Maintenance tasks
- `ci`: CI/CD changes

**Scope suggestions:** `ecs`, `plugin`, `service`, `foundation`, `game`, `auth`,
`gateway`, `lobby`, `dbproxy`, `build`, `docs`.

## Code Standards

### C++ Style

- Use C++20 features when beneficial
- Follow existing code style (clang-format 21+ configuration provided in `.clang-format`)
- Prefer RAII for resource management
- Use `auto` for obvious types
- Avoid raw pointers; use smart pointers
- Prefer `Result<T, E>` over exceptions for error handling
- Prefer standard library over custom implementations

### Naming Conventions

- **Classes/Structs**: `snake_case`
- **Functions/Methods**: `snake_case`
- **Variables**: `snake_case`
- **Constants**: `UPPER_SNAKE_CASE`
- **Template Parameters**: `PascalCase`
- **Namespaces**: `cgs::core`, `cgs::ecs`, `cgs::plugin`, etc.

Detailed standards: [`docs/contributing/CODING_STANDARDS.md`](docs/contributing/CODING_STANDARDS.md)

### Documentation

Use Doxygen-style comments:

```cpp
/**
 * @brief Brief description
 * @param param Parameter description
 * @return Return value description
 * @thread_safety Thread-safe / Not thread-safe
 */
auto function(int param) -> int;
```

Writing guidelines: [`docs/contributing/DOCUMENTATION_GUIDELINES.md`](docs/contributing/DOCUMENTATION_GUIDELINES.md)

## Testing

### Test Requirements

All code contributions must include tests:

- **Unit tests**: Test individual components (`tests/unit/`)
- **Integration tests**: Test component interactions (`tests/integration/`)
- **Benchmarks**: Performance-critical paths (`tests/benchmark/`)

### Writing Tests

Use Google Test framework:

```cpp
#include <gtest/gtest.h>

TEST(ComponentTest, BasicFunctionality) {
    // Test implementation
}
```

### Test Coverage

Aim for:
- New code: > 80% coverage
- Critical paths: 100% coverage
- Error handling: Test failure scenarios

## Pull Requests

### PR Checklist

Before submitting a PR, ensure:

- [ ] Code compiles without warnings
- [ ] All tests pass (`ctest --preset conan-release`)
- [ ] New tests added for new functionality
- [ ] Documentation updated
- [ ] Code follows project style (`clang-format --dry-run --Werror`)
- [ ] Commit messages follow conventional format
- [ ] No merge conflicts with main branch
- [ ] CHANGELOG.md entry added under `[Unreleased]`

### Review Process

1. Automated checks run (CI, lint, coverage, docs)
2. Maintainer reviews code
3. Address review comments
4. Approved PR is merged (squash merge preferred)

## Getting Help

- Check [existing issues](https://github.com/kcenon/common_game_server/issues)
- Open a [discussion](https://github.com/kcenon/common_game_server/discussions) for questions
- Read the [FAQ](docs/guides/FAQ.md)

## License

By contributing, you agree that your contributions will be licensed under the
BSD 3-Clause License. See [LICENSE](LICENSE) for details.
