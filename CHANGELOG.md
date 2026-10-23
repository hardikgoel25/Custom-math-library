# Changelog

## 2.0.0 — complete rewrite

### Library
- New C++14 library `cml` replacing the C/C++ mix: `core`, `special`, `expression`,
  `polynomial`, `matrix`, `roots`, `optimize`, `integrate`, `ode`, `interpolate`, `stats`.
- Elementary functions rewritten with proper range reduction (Cody–Waite trig,
  2^k·e^r exponentials, mantissa-split logarithms), IEEE special cases, and
  convergence checks; verified against mpmath at 40 digits.
- Fixed legacy defects: assignment-instead-of-comparison in `iPower`/`rPower`,
  functions falling off the end without returning, `arcCosine`/`arcSecant`/
  `arcCosecant` wrong domains, `Sqrt` with 10 000 unconditional iterations,
  exponential-time recursive Fibonacci in `FibonacciMin`, uninitialised `fx` in
  `NewtonMin`, duplicate `FnData` definitions across modules (ODR violation),
  `#define abs` shadowing `std::abs`, unchecked `fscanf`/`fread` sizes and raw
  pointer management in `Matrix`/`Poly`, `std::pow(-1, i+j)` cofactor sign.
- Matrices: LU/QR/Cholesky/RREF/eigen decompositions, least squares, Strassen,
  size caps and bounds checks.
- Polynomials: text parser, all complex roots (Aberth–Ehrlich), gcd, composition.
- Expression parser with implicit multiplication and symbolic differentiation.
- New modules: quadrature (9 rules incl. Romberg, Gauss–Legendre, adaptive,
  improper integrals), ODE solvers (Euler…RK45), interpolation (Lagrange, Newton,
  splines, polyfit), statistics.

### Tooling
- JSON compute engine with validation and resource caps; C ABI; `cml-cli`.
- WebAssembly build; Node gateway with rate limiting, timeouts and security headers.
- React + TypeScript web UI with interactive plots for every module.
- CMake and Makefile builds, GitHub Actions CI (gcc/clang/msvc, sanitizers,
  WASM, Docker, GitHub Pages), Dockerfile.
- 37 test suites with 61 000+ checks.
