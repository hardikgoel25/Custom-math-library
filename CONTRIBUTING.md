# Contributing

Thanks for helping improve CML. The project values correctness and clarity over cleverness.

## Setup

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ./build/cml-tests
cd web && npm ci && npm run dev
```

The Makefile works where CMake is unavailable (`make test`). On 32-bit MinGW the
build adds `-mfpmath=sse` automatically; the bit-exact algorithms need it.

## Guidelines

- **No `<cmath>` in `src/core.cpp` or `src/special.cpp`.** The point of the library
  is the from-scratch implementations. Other modules use `cml::` functions too.
- **Tests first.** Every numerical routine gets a test against a known value
  (closed form, mpmath reference, or `<cmath>` where the comparison is meaningful).
  Regenerate `tests/core_reference.inc` with mpmath if you add reference cases.
- **No printing, no globals, no macros** in library code. Report failures through
  result structs (`converged`, `message`) or exceptions for invalid arguments.
- **Engine ops** need: parameter validation with a named error, a cap on every
  count, an entry in `registry()` with a description and param schema, a contract
  test in `tests/test_engine.cpp`, and a row in `docs/API.md`.
- **UI** pages keep their state in the URL (`useUrlState`) so results are shareable,
  render engine errors with `ErrorBanner`, and never use `innerHTML`.
- Keep warnings at zero (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`, `/W4`).

## Commit conventions

Conventional commits: `feat:`, `fix:`, `docs:`, `test:`, `build:`, `ci:`, `refactor:`.
Do not add AI attribution lines (`Co-Authored-By` etc.) to commits or PRs.
