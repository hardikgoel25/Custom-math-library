# Architecture

```
                ┌──────────────────────────────────────────────────────────┐
                │                    C++14 library  (namespace cml)        │
                │  core · special · expression · polynomial · matrix       │
                │  roots · optimize · integrate · ode · interpolate · stats│
                └───────────────┬──────────────────────────────────────────┘
                                │
                     ┌──────────▼───────────┐
                     │  JSON engine          │   execute({"op": ..., "params": ...})
                     │  (engine.cpp)         │   validation · resource caps · never throws
                     └──────────┬───────────┘
                                │  C ABI: cml_execute / cml_free / cml_version
          ┌─────────────────────┼─────────────────────────┬────────────────────┐
          ▼                     ▼                         ▼                    ▼
  WebAssembly (emcc)     native cml-cli            libcml.a / cml.dll      your FFI
  web/public/engine      one-shot · --stdin        link from C/C++         (Python ctypes,
  runs in a Web Worker   JSON-lines server         (include/cml/*.hpp)     Rust, Go, …)
          │                     │
          ▼                     ▼
  React UI (web/)        Node gateway (apps/server/)  ──  POST /api/compute, serves web/dist
```

## How C++ runs "on the backend"

There is no single answer; the engine was designed so the **same C++ source** runs
in every tier, and the deployment picks whichever fits:

| Mode | Where the C++ executes | When to use it |
|---|---|---|
| **WASM in the browser** (default) | Inside a Web Worker on the visitor's machine. The UI is a static site; no server does any math. | Static hosting (GitHub Pages, Netlify, Vercel, S3). Zero cost, scales with the client, works offline after the first load. |
| **WASM in Node** | `apps/server` loads `cml.js` in `worker_threads`. Memory-safe sandbox; a runaway request is terminated by killing the thread. | A hosted JSON API without native toolchains in the image. |
| **Native process pool** | `apps/server` spawns `cml-cli --stdin` processes (JSON-lines over stdio, no shell). Timeouts kill and respawn workers. | Maximum throughput for heavy jobs; the Docker image includes the binary. |
| **Linked library** | `libcml.a` or the shared `cml` library via `include/cml/*.hpp` or the C ABI. | Embedding in another C++/Python/Rust service. |

Every mode speaks the same request/response contract, so the UI only needs an
`execute(request)` function (`web/src/engine/client.ts`) that prefers WASM and
falls back to `POST /api/compute`.

## Safety model

- **Library**: no raw `new`/`delete`, bounds-checked indexing (`std::out_of_range`),
  allocation caps on matrices/polynomials/data, no printing, no global mutable
  state, convergence checks instead of fixed iteration counts, IEEE-correct NaN/Inf
  handling.
- **Engine**: strict JSON parser (depth 64, 4 MB), every parameter type- and
  range-checked with a named error, iteration/sample/dimension limits, every
  exception converted into `{ok:false, error:{code,...}}`.
- **Gateway**: JSON body limit, per-IP token bucket, per-request timeout with
  worker kill, CORS allow-list, strict CSP and other security headers,
  path-traversal-safe static serving, no shell execution, unprivileged container user.
- **UI**: CSP in `index.html`, no `innerHTML` with data (React text nodes only),
  WASM in a worker with a watchdog, `localStorage` only for conveniences.

## Repository layout

```
include/cml/   public headers (one per module) + engine.hpp + c_api.h
src/           implementations (+ wasm_entry.cpp for Emscripten)
apps/cli/      cml-cli: JSON one-shot, --stdin server, eval, repl
apps/server/   zero-dependency Node gateway (engine pools, rate limit, static)
web/           Vite + React + TypeScript UI (engine client, plots, pages)
wasm/          Emscripten build scripts
tests/         C++ unit tests (own 100-line framework) + mpmath reference values
docker/        multi-stage Dockerfile and compose file
docs/          this folder
legacy/        the original pre-rewrite sources, for reference
```

## Numerical notes

- `core`: sin/cos reduce by π/2 with a three-part constant and an exact
  `TwoProd`, so arguments up to ~1e15 stay accurate; exp reduces by ln 2 into
  `2^k · e^r`; ln splits the mantissa and uses the `2·atanh((m−1)/(m+1))`
  series; sqrt seeds from the exponent bits and runs Newton to convergence.
  Accuracy is verified against mpmath at 40 digits (`tests/core_reference.inc`).
- `matrix`: LU with partial pivoting for det/solve, Gauss–Jordan for the
  inverse, Householder QR, Cholesky, Jacobi rotations for symmetric eigenproblems,
  Hessenberg reduction + Wilkinson-shifted QR for general eigenvalues.
- `polynomial`: Aberth–Ehrlich simultaneous iteration from a Cauchy-bound circle,
  Newton polishing, real-axis snapping; stable quadratic formula for degree 2.
- `roots`/`optimize`: Brent–Dekker and Brent's parabolic minimiser follow the
  classic formulations; Newton uses the symbolic derivative from the expression
  parser when it exists.
- `integrate`: Gauss–Legendre nodes are computed by Newton on the Legendre
  recurrence and cached; `integrate()` maps infinite ranges with
  `x = a + t/(1−t)` and `x = t/(1−t²)`.
- `ode`: Dormand–Prince 5(4) with FSAL and PI-free step control.
