# Revamp Prompt — Custom Math Library → CML (C++ core + WebAssembly + modern web UI)

> This is the prompt/spec the revamp was executed against. Hand it to a developer or an AI agent
> to reproduce, extend, or audit the work.

## Role

You are a senior numerical-computing engineer and product-minded front-end developer. Revamp the
repository `hardikgoel25/Custom-math-library` (a from-scratch C/C++ math library with elementary
functions, matrices, polynomials, root finding and 1-D minimisation) into a production-grade,
deployable numerical toolkit with a modern, accessible, graph-rich web UI. Keep the educational
spirit: every algorithm is implemented by hand, without `<cmath>` in the core, but everything must
be correct, safe, and tested against the standard library.

## Deliverables

1. **C++14 library `cml`** (namespace `cml`, headers in `include/cml/`, sources in `src/`), portable
   across GCC ≥ 6, Clang, MSVC and Emscripten. Zero third-party dependencies.
2. **JSON compute engine**: a single entry point `execute({"op": "...", "params": {...}})` that
   dispatches to every library feature, validates input, enforces resource caps, and never crashes.
   Expose it as a C ABI (`cml_execute`, `cml_free`, `cml_version`) so it can be loaded from
   WebAssembly, Node, Python (ctypes) or any FFI.
3. **CLI** (`cml-cli`): one-shot JSON, JSON-lines stdin server mode, expression evaluator, REPL.
4. **WebAssembly build** (Emscripten) of the engine, runnable in the browser (inside a Web Worker
   with a watchdog timeout) and in Node.
5. **Node gateway server** (zero npm dependencies): serves the built UI and `POST /api/compute`,
   drives the engine (WASM in a worker thread, or a pool of native `cml-cli --stdin` processes),
   with body-size limits, per-IP rate limiting, request timeouts, CORS allow-list and security
   headers.
6. **Web UI** (Vite + React + TypeScript, hand-written CSS design system): dashboard plus one page
   per module, every result visualised (function plots, iteration tables, convergence charts,
   shaded integrals, ODE trajectories/phase plots, complex-plane root plots, matrix heat cells,
   histograms, 2-D optimisation paths), dark/light theme, command palette, keyboard navigation,
   ARIA, reduced-motion support, mobile layout, shareable URLs, local history, CSV/JSON/PNG export,
   engine status indicator.
7. **Tests**: C++ unit tests for every module (compare against `<cmath>`/known values, edge cases,
   NaN/Inf, malformed JSON, resource caps), engine contract tests, web unit tests.
8. **Build/Deploy**: CMake (+ Makefile fallback), Emscripten build script, Dockerfile
   (multi-stage: build engine → build UI → slim runtime), docker-compose, GitHub Actions
   (build+test matrix, WASM + UI build, GitHub Pages deploy, Docker image).
9. **Docs**: README, ARCHITECTURE, API reference for every op, SECURITY, DEPLOYMENT, CONTRIBUTING,
   CHANGELOG, MIT licence. Keep the original sources in `legacy/` for reference.

## Library scope (expand and fix)

- **core**: constants; abs/sign/floor/ceil/round/trunc/fmod; ipow/pow/sqrt/cbrt/hypot; exp/exp2/
  ln/log2/log10/log(base); sin/cos/tan + reciprocals; asin/acos/atan/atan2 + reciprocals;
  sinh…coth; asinh…acoth; deg/rad. Use proper range reduction (Cody–Waite for trig, 2^k·e^r for
  exp, mantissa/exponent split + atanh series for ln), IEEE special-case handling, convergence
  checks instead of fixed iteration counts.
- **special**: gamma/lgamma (Lanczos), beta, erf/erfc, normal pdf/cdf, factorial, binomial,
  permutations, gcd/lcm, primality, prime factors, Fibonacci.
- **matrix**: `std::vector` storage, bounds-checked access, overflow-safe allocation with size
  caps, +,−,×, scalar ops, transpose, trace, norms, LU with partial pivoting (det/inverse/solve),
  Gauss–Jordan RREF, rank, QR (Householder), Cholesky, symmetric eigen (Jacobi), general
  eigenvalues (Hessenberg + shifted QR), power iteration, least squares, Strassen for large square
  products, text parsing/formatting.
- **polynomial**: `std::vector` coefficients, string parser (`3x^2 - 2x + 1`), Horner evaluation
  (real/complex, with derivative), arithmetic incl. divmod, gcd, power, composition, deflation,
  derivative/integral, all complex roots (Aberth/Durand–Kerner + Newton polish), real roots,
  pretty printing.
- **expression**: Pratt parser for `f(x)`, `f(x,y)`, `f(t,y)` with implicit multiplication,
  ~40 built-in functions, constants, symbolic differentiation with simplification, free-variable
  detection, compile to a fast closure. Depth and length limits.
- **roots**: bisection, regula falsi (Illinois), secant, Newton (analytic or numeric derivative),
  Halley, Brent, Ridders, linear and variable-step search, find-all-roots scan. Each returns
  root, f(root), iterations, evaluations, converged flag, message, and full iteration history.
- **optimize**: bracket, golden section, Fibonacci (iterative), Brent, Newton, binary (derivative
  bisection), linear/variable-step; N-D Nelder–Mead and gradient descent with numeric gradients;
  histories/paths for plotting; maximise by negation.
- **integrate**: midpoint, trapezoid, Simpson 1/3 and 3/8, Romberg (table), Gauss–Legendre
  (computed nodes), adaptive Simpson, Monte Carlo; error estimates and evaluation counts.
- **ode**: Euler, Heun, midpoint, RK4, Dormand–Prince RK45 adaptive; scalar and systems.
- **interpolate**: linear, Lagrange, Newton divided differences (table), natural cubic spline,
  least-squares polynomial fit.
- **stats**: descriptive statistics, quantiles, skew/kurtosis, covariance/correlation, linear
  regression, histogram, z-scores, moving average.

## Non-functional requirements

- **Safety**: no raw `new`/`delete`, no unchecked indexing, no macros that shadow standard names
  (`abs`), no uninitialised reads, no duplicate struct definitions across modules (the legacy
  `FnData` ODR violation), checked numeric parsing, hard caps on iterations/samples/sizes, JSON
  depth/size limits, no `eval` anywhere, strict CSP in the UI, no `innerHTML` with user data.
- **Determinism**: pure functions, no global state, no printing from library code.
- **Accuracy**: core functions within a few ULP of `<cmath>` over tested ranges; document limits.
- **Accessibility**: WCAG 2.1 AA contrast, keyboard operable, focus visible, labelled controls,
  text alternatives for plots, `prefers-reduced-motion` and `prefers-color-scheme` respected.
- **Performance**: plotting 2k points and typical ops under 10 ms in WASM; worker keeps UI
  responsive; long computations cancellable via watchdog.
- **DX**: one-command build and test; clear error messages with positions for parser errors.

## Commit conventions

Conventional commits (`feat:`, `fix:`, `docs:`, `build:`, `ci:`, `test:`). No AI attribution
lines in commits, PRs or git metadata.
