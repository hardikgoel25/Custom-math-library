# Security

## Threat model

The engine evaluates untrusted mathematical input (expressions, matrices, data
sets) from the web. The goals are: no memory-safety bugs reachable from input, no
way to exhaust CPU or memory, no code execution, and no cross-site attacks on the UI.

## Library and engine

| Risk | Mitigation |
|---|---|
| Buffer overflows / UB from indexing | `std::vector` everywhere, bounds-checked `Matrix::operator()`, no raw `new`/`delete`, ASan/UBSan job in CI |
| Allocation bombs (`Matrix(1e9, 1e9)`, huge polynomials) | overflow-checked size computation and hard caps (`Matrix::set_max_elements`, degree ≤ 300 in the engine, data ≤ 500k) |
| CPU exhaustion (`maxIter: 1e12`, `samples: 1e9`, deep recursion) | every count is range-checked; adaptive routines cap evaluations and depth; expression parser limits length (20k), depth (256) and node count (50k); JSON parser limits depth (64) and size (4 MB) |
| Non-finite inputs (NaN/Inf) corrupting algorithms | parameters are validated as finite where required; routines detect non-finite values and stop with a message instead of looping |
| Macro / ODR hazards from the legacy code (`#define abs`, duplicate `FnData`) | removed; one namespace, no macros that shadow std names |
| Exceptions escaping the C ABI | `cml_execute` catches everything and returns a JSON error; the WASM build uses native exception handling |

## Gateway (`apps/server`)

- Zero third-party packages: nothing to audit but Node itself.
- `Content-Type` must be JSON; body limited to 256 KB; declared and actual length both checked.
- Per-IP token-bucket rate limit with bounded memory.
- Per-request timeout; a stuck worker (thread or process) is killed and respawned.
- Engine processes are spawned without a shell and only ever receive JSON on stdin.
- CORS is same-origin unless an explicit allow-list is configured.
- Security headers on every response: CSP, `X-Content-Type-Options`,
  `X-Frame-Options`, `Referrer-Policy`, `Permissions-Policy`, COOP/CORP, optional HSTS.
- Static files are resolved inside the build directory only (path traversal
  tests included); unknown extensions are served as octet-stream.
- Docker image runs as an unprivileged user with a read-only filesystem.

## Web UI

- Content-Security-Policy meta tag; scripts only from the same origin
  (`'wasm-unsafe-eval'` is required for WebAssembly instantiation).
- No `innerHTML` / `dangerouslySetInnerHTML`; all engine output is rendered as text.
- WebAssembly runs in a Web Worker with a watchdog that terminates runaway computations.
- `localStorage` holds only UI preferences and a local history; nothing sensitive.

## Reporting

Open a GitHub issue marked "security" or email the maintainer. Please include a
minimal request that reproduces the problem.
