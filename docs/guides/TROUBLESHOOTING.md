# Troubleshooting

This guide covers common issues encountered when building, running, or
deploying `common_game_server`. For Doxygen-rendered troubleshooting,
see [`../troubleshooting.dox`](../troubleshooting.dox).

## Build Issues

### `yaml-cpp not found` during CMake configure

**Cause**: Conan dependencies were not installed before running CMake.

**Fix**:
```bash
conan install . --output-folder=build --build=missing -s build_type=Release
cmake --preset conan-release
```

### `kcenon::common_system not found`

**Cause**: kcenon ecosystem dependencies are not available — neither via
package manager nor FetchContent.

**Fix options**:
- Use the `default` preset, which fetches via `FetchContent` from GitHub
- Use the `conan-release` preset and configure Conan to find kcenon packages
- Pre-clone the kcenon repos and set `FETCHCONTENT_SOURCE_DIR_*` env vars

### `clang-format` mismatch in CI

**Cause**: Local clang-format version differs from CI's pinned version (21.1.8).

**Fix**: Install clang-format 21+ locally:
- Ubuntu: `sudo apt install clang-format-21`
- macOS: `brew install llvm@21 && brew link --force --overwrite llvm@21`

Then format:
```bash
find include/cgs src -name '*.hpp' -o -name '*.cpp' | xargs clang-format -i
```

### `Compiler doesn't support C++20`

**Cause**: Compiler is too old (GCC 10 / Clang 13 or earlier).

**Fix**: Upgrade to GCC 11+, Clang 14+, Apple Clang 14+, or MSVC 2022+.

### Sanitizer linker errors

**Cause**: System libraries not built with sanitizer support, or compiler/runtime mismatch.

**Fix**:
- Ubuntu: `sudo apt install libasan6 libubsan1`
- Use the same compiler family throughout the build (don't mix GCC and Clang)

## Runtime Issues

### Plugin fails to load: `error_code::plugin_load_failed`

**Cause options**:
1. The shared library doesn't export `cgs_create_plugin`
2. The plugin was built with a different C++ standard library (libstdc++ vs libc++)
3. A required dependency `.so` is missing

**Fix**:
- Verify export with `nm -D libmy_plugin.so | grep cgs_create_plugin`
- Use `ldd libmy_plugin.so` (Linux) or `otool -L libmy_plugin.dylib` (macOS) to check missing deps
- Rebuild the plugin with the same toolchain as the server

### Plugin fails to load: `error_code::plugin_abi_mismatch`

**Cause**: The plugin's `PLUGIN_ABI_VERSION` doesn't match the framework's.

**Fix**: Rebuild the plugin against the current `<cgs/plugin/plugin_manager.hpp>`.

### Database connection refused

**Cause**: PostgreSQL is not reachable or credentials are wrong.

**Fix**:
1. Verify PostgreSQL is running: `pg_isready -h <host> -p <port>`
2. Check the DBProxy config for correct host/port/credentials
3. Verify firewall allows the DBProxy node to reach PostgreSQL
4. Check the PostgreSQL log for connection rejection reasons

### World tick budget exceeded

**Symptom**: GameServer logs `tick budget exceeded: 65 ms (target 50 ms)`.

**Cause**: A system is too slow, or there are too many entities.

**Fix**:
1. Profile with `tests/benchmark/ECS_*` to identify the slow system
2. Convert `each` to `par_each` if the system has no inter-entity dependencies
3. Reduce entity count or split into multiple GameServer instances
4. Verify CPU is not contended (cgroups, taskset, other processes)

### JWT verification fails

**Cause**: AuthServer's private key and Gateway's public key don't match.

**Fix**: Regenerate the keypair on AuthServer and copy the public key to Gateway:
```bash
openssl genrsa -out config/keys/jwt_private.pem 2048
openssl rsa -in config/keys/jwt_private.pem -pubout -out config/keys/jwt_public.pem
# Copy jwt_public.pem to all services that verify tokens
```

### Service exits immediately on startup

**Cause options**:
- Config file path is wrong (`--config /path/to/config.yaml`)
- Required environment variable is missing
- Port is already in use

**Fix**: Run with `--log-level debug` to see the startup sequence and the
exact error.

## Test Issues

### `ctest` reports "No tests were found"

**Cause**: Tests were not built (`CGS_BUILD_TESTS=OFF`).

**Fix**:
```bash
cmake --preset conan-release -DCGS_BUILD_TESTS=ON
cmake --build --preset conan-release
ctest --preset conan-release
```

### Tests pass locally but fail in CI

**Common causes**:
- Test depends on a hard-coded local path
- Test assumes a specific locale (LC_ALL)
- Race condition that only appears under CI's scheduling
- Test fixture file not committed to git

**Fix**: Run the failing test under `tsan` locally to catch races, and verify
the test does not depend on environment-specific state.

## CI Issues

### CI workflow fails with TLS certificate errors

**Cause**: Sandbox network restrictions in some CI environments.

**Fix**: This is a CI environment issue, not a project issue. Report it to
the workflow maintainer.

### Doxygen workflow fails with "missing parameter documentation"

**Cause**: `WARN_NO_PARAMDOC` is enabled and a public function is missing
`@param` documentation.

**Fix**: Add `@param name Description` for every parameter, or remove the
parameter if it's truly unused.

## Deployment Issues

### Kubernetes pods stuck in `Pending`

**Cause**: Insufficient cluster resources or scheduling constraints.

**Fix**: Check `kubectl describe pod <name>` for the reason. Common fixes:
- Add nodes to the cluster
- Reduce resource requests in the Deployment manifest
- Verify nodeSelector / tolerations / affinity

### Pods crash with `OOMKilled`

**Cause**: Memory limit too low for the workload.

**Fix**: Increase the memory limit in the Deployment manifest, or reduce
in-process state.

### Health probe failures

**Cause**: Service responds to `/health` slowly during startup.

**Fix**: Increase `initialDelaySeconds` on the liveness probe.

## Getting More Help

- Read the [FAQ](FAQ.md)
- Search [GitHub Issues](https://github.com/kcenon/common_game_server/issues)
- Open a new issue with:
  - OS, compiler version, CMake version
  - Full error message
  - Reproduction steps
  - Output of `cmake --version` and `git rev-parse HEAD`

## See Also

- [`FAQ.md`](FAQ.md) — Frequently asked questions
- [`BUILD_GUIDE.md`](BUILD_GUIDE.md) — Build instructions
- [`../troubleshooting.dox`](../troubleshooting.dox) — Doxygen-rendered version
- [`DEPLOYMENT_GUIDE.md`](DEPLOYMENT_GUIDE.md) — Deployment troubleshooting
