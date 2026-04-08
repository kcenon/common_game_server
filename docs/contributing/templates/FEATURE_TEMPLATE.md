# Feature: <Feature Name>

> **Status**: Proposed | Implemented | Deprecated
> **Owner**: <name>
> **Tracking issue**: <#NNN>

## Summary

One paragraph describing what this feature does and why it exists.

## Motivation

Why is this feature needed? What user problem does it solve?

## API

Public API surface (headers, functions, types):

```cpp
namespace cgs::module {
    class new_thing {
    public:
        Result<void> do_something();
    };
}
```

## Usage Example

```cpp
auto t = cgs::module::new_thing();
auto r = t.do_something();
if (!r) {
    // handle error
}
```

## Implementation Notes

- Where the implementation lives (`src/...`)
- Key data structures
- Performance characteristics (Big-O, memory)
- Thread safety guarantees
- Failure modes

## Testing Strategy

- Unit tests: `tests/unit/...`
- Integration tests: `tests/integration/...`
- Benchmarks (if performance-critical): `tests/benchmark/...`

## Documentation

Files updated:
- [ ] `docs/FEATURES.md` — feature matrix entry
- [ ] `docs/API_REFERENCE.md` — API surface documentation
- [ ] `docs/ARCHITECTURE.md` — if architecture is affected
- [ ] `docs/CHANGELOG.md` — release note entry
- [ ] Doxygen comments on all public symbols

## Open Questions

List any unresolved design questions here.

## See Also

- Related features
- Related ADRs
- Upstream design references
