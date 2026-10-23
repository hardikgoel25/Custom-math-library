# CML · Custom Math Library

A from-scratch numerical computing toolkit in C++14, with a JSON compute engine that
runs as **WebAssembly in the browser**, as a **native CLI**, behind a **Node API**, or
linked into your own program. Comes with an interactive web UI that plots every
algorithm as it works.

> Every elementary function (`sin`, `exp`, `ln`, `gamma`, `erf`, …) and every
> numerical method is implemented by hand without the C math library, then
> verified against high-precision references.

**Live UI** (after enabling GitHub Pages in this repo): `https://<user>.github.io/Custom-math-library/`

## What's inside

| Module | Highlights |
|---|---|
| `core` | pow, sqrt, cbrt, exp/ln family, trig + inverse, hyperbolic + inverse, IEEE special cases, Cody–Waite range reduction, ~1 ULP accuracy |
| `special` | gamma, lgamma, digamma, beta, erf/erfc, normal pdf/cdf/quantile, factorials, binomials, gcd/lcm, deterministic Miller–Rabin primality, factorisation, sieve, Fibonacci |
| `expression` | parser for `3x^2 - sin(2x)/x` with implicit multiplication, 60 functions, constants, **symbolic differentiation**, compiled evaluation |
| `polynomial` | text parser, Horner evaluation, calculus, arithmetic incl. division and gcd, composition, **all complex roots** (Aberth–Ehrlich) |
| `matrix` | LU (det, inverse, solve), Householder QR, Cholesky, RREF/rank, Jacobi + shifted-QR eigenvalues, least squares, Strassen, size caps |
| `roots` | bisection, regula falsi (Illinois), secant, Newton (symbolic f′), Halley, Brent, Ridders, Steffensen, fixed point, step searches, find-all |
| `optimize` | golden section, Fibonacci, Brent, Newton, binary, step searches; Nelder–Mead and gradient descent in N dimensions — all with histories |
| `integrate` | midpoint, trapezoid, Simpson 1/3 & 3/8, Boole, Romberg, Gauss–Legendre (computed nodes), adaptive Simpson, Monte Carlo, improper integrals |
| `ode` | Euler, Heun, midpoint, RK4, adaptive Dormand–Prince RK45 for scalar equations and systems |
| `interpolate` | Lagrange, Newton divided differences, piecewise linear, natural/clamped cubic splines, least-squares polynomial fit |
| `stats` | descriptive statistics, quantiles, skew/kurtosis, correlation (Pearson/Spearman), regression, histograms, transforms |
| `engine` | one JSON entry point for everything, strict validation, resource caps, never crashes |

## Quick start

```bash
# library + CLI + tests (CMake ≥ 3.14, any C++14 compiler)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/cml-tests                                   # 37 suites, 61k checks
./build/cml-cli eval "gamma(5.5) / sqrt(2pi)"
./build/cml-cli --pretty '{"op":"roots.find","params":{"f":"cos(x) - x","method":"brent","a":0,"b":1}}'
```

```bash
# web UI with the WebAssembly engine (needs Emscripten + Node 18+)
source ~/emsdk/emsdk_env.sh && ./wasm/build.sh
cd web && npm ci && npm run dev                      # http://localhost:5173
```

```bash
# everything in one container (UI + JSON API)
docker build -f docker/Dockerfile -t cml . && docker run --rm -p 8080:8080 cml
```

No CMake? `make test` works with plain GCC/Clang/MinGW.

## Using the library

```cpp
#include <cml/cml.hpp>
using namespace cml;

double y = sin(1.0) + gamma(5.5);                       // from-scratch functions
Expression f = Expression::parse("x^3 - x - 2");
RootResult r = newton(f.as_function(), f.derivative().as_function(), 1.5);
Matrix A{{4, 1}, {1, 3}};
auto e = A.eigen_symmetric();                            // e.values, e.vectors
IntegralResult I = integrate([](double x) { return exp(-x * x); }, -inf(), inf());
OdeResult sol = rk45([](double, const std::vector<double>& s) { return std::vector<double>{s[1], -s[0]}; }, 0, {1, 0}, 10);
```

From any other language, call the C ABI:

```c
char* out = cml_execute("{\"op\":\"integrate.compute\",\"params\":{\"f\":\"sin(x)\",\"a\":0,\"b\":\"pi\"}}");
/* {"ok":true,"op":"integrate.compute","result":{"value":2,...}} */
cml_free(out);
```

## How does C++ run on the backend?

The engine is compiled once and hosted three ways (details in
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)):

1. **In the browser** — Emscripten builds `cml.wasm`; the UI runs it inside a Web
   Worker with a watchdog. The site is fully static, so it deploys to GitHub Pages or
   any CDN for free and works offline after the first load.
2. **Node gateway** — `apps/server` (zero dependencies) exposes `POST /api/compute`.
   It runs the same WASM in worker threads, or a pool of native `cml-cli --stdin`
   processes, with body limits, rate limiting, timeouts that kill stuck workers,
   CORS allow-list and strict security headers. The Dockerfile packages it.
3. **Linked** — `libcml.a` / shared library via the headers or `cml_execute()`.

## Documentation

- [docs/API.md](docs/API.md) — every engine op and its parameters
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — design, safety model, numerical notes
- [docs/DEPLOYMENT.md](docs/DEPLOYMENT.md) — static hosting, Docker, GitHub Pages
- [docs/SECURITY.md](docs/SECURITY.md) — threat model and mitigations
- [docs/REVAMP_PROMPT.md](docs/REVAMP_PROMPT.md) — the specification this rewrite was built from
- [CONTRIBUTING.md](CONTRIBUTING.md), [CHANGELOG.md](CHANGELOG.md)

The original sources are preserved under [`legacy/`](legacy/) for comparison.

## License

MIT © 2025 Hardik Goel
