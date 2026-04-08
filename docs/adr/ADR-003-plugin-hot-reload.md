---
doc_id: "CGS-ADR-003"
doc_title: "ADR-003: Plugin Hot Reload (Development Only)"
doc_version: "1.0.0"
doc_date: "2026-04-08"
doc_status: "Accepted"
project: "common_game_server"
category: "ADR"
---

# ADR-003: Plugin Hot Reload (Development Only)

> **SSOT**: This document is the single source of truth for **ADR-003**.

| Field | Value |
|-------|-------|
| Status | Accepted |
| Date | 2026-02-03 |
| Decision Makers | kcenon ecosystem maintainers |

## Context

Game development iterates rapidly: a designer might tweak a combat formula,
recompile, and want to see the result without restarting the world. Restarting
the server loses all in-memory state (active sessions, world data, plugin state).

`common_game_server` plugins are shared libraries (`.so` / `.dll` / `.dylib`)
loaded via `dlopen` / `LoadLibrary`. The question is whether to support
**hot reload** — unloading the old version and loading the new version without
restarting the server.

Tradeoffs:

| Concern | With hot reload | Without hot reload |
|---------|----------------|-------------------|
| Iteration speed | Fast — recompile, watch reload | Slow — full restart per change |
| Production stability | Risky — code-swap can corrupt state | Stable — no in-place modification |
| Security | Risky — file watcher is an attack surface | Safe |
| State migration complexity | High — must serialize/deserialize | None |
| ABI churn risk | High — `vtable` layout must match | None |

## Decision

**Support hot reload, but only when `CGS_HOT_RELOAD=ON`. Production builds
must set `CGS_HOT_RELOAD=OFF`.**

Specifically:

1. **CMake option `CGS_HOT_RELOAD`** (default `OFF`) gates the entire feature
2. **File watcher** monitors the `plugins/` directory using `inotify` (Linux),
   `kqueue` (macOS), or `ReadDirectoryChangesW` (Windows)
3. **State migration** — Plugins implement `serialize`/`deserialize` hooks
   that snapshot their state to a flat byte buffer before unload, and restore
   it after the new version is loaded
4. **ABI version check** — The new shared library's `cgs::plugin::PLUGIN_ABI_VERSION`
   macro must match the running framework. Mismatch → reload aborted, old
   plugin remains active
5. **Failure recovery** — If `on_load` of the new version fails, the old
   version is restored from the serialized state
6. **Production lockout** — `CGS_HOT_RELOAD=OFF` removes the file watcher,
   the `dlopen` of replacement libraries, and the serialize/deserialize
   plumbing entirely. Zero attack surface in production binaries.

## Alternatives Considered

### Hot reload always on

- **Pros**: Same code path in dev and prod; one less build configuration.
- **Cons**: Production attack surface; production binaries shouldn't have
  arbitrary `dlopen` capability.

### Hot reload via separate "dev server" binary

- **Pros**: Cleanly separates dev and prod artifacts.
- **Cons**: Two binaries to maintain; risk of divergence between dev and
  prod behavior.

### No hot reload, fast restart instead

- **Pros**: Simplest implementation.
- **Cons**: Fast restart is not actually fast — game state regeneration,
  database reconnection, and TCP listener rebinding take seconds. Iteration
  loop suffers.

## Consequences

### Positive

- **Fast iteration** — Designers can edit and reload in <100 ms (excluding compile time)
- **Production safety** — Production binaries have no hot-reload code at all
- **State preservation** — Plugins can survive reloads without losing data
- **ABI safety** — Version mismatch is caught before harm

### Negative

- **Two build configurations** — `CGS_HOT_RELOAD=ON` for dev, `OFF` for prod.
  Mitigated by CMake presets making this explicit.
- **Plugin authoring overhead** — Plugins should implement `serialize`/`deserialize`
  to benefit from hot reload. Optional for plugins that don't need state preservation.
- **State migration bugs** — A plugin author can introduce bugs in their
  serialize/deserialize logic. Mitigated by mock testing in `tests/`.

## Implementation Notes

- File watcher debounces events to avoid reloading mid-write
- Reload is **synchronous** — the world tick is paused for the swap
- Reload happens at tick boundaries to avoid mid-system mutation
- Failure recovery is best-effort: if both old and new fail, the plugin is unloaded entirely

## Security

Hot reload is **off by default** and only enabled in development builds.
Production deployments must NOT enable `CGS_HOT_RELOAD`. The CMake build
emits a warning if `CGS_HOT_RELOAD=ON` is combined with `CMAKE_BUILD_TYPE=Release`.

## References

- [`../guides/PLUGIN_DEVELOPMENT_GUIDE.md`](../guides/PLUGIN_DEVELOPMENT_GUIDE.md) — Plugin authoring
- [`../advanced/PLUGIN_INTERNALS.md`](../advanced/PLUGIN_INTERNALS.md) — Internal mechanism (when added)
- [`../FEATURES.md#plugins-detail`](../FEATURES.md) — User-facing plugin features
- [`../SECURITY.md`](../SECURITY.md) — Security policy
