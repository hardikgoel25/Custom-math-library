# Engine API reference

Every feature is reachable through one call:

```json
{ "op": "<category.name>", "params": { ... }, "id": "optional, echoed back" }
```

Responses:

```json
{ "ok": true,  "op": "roots.find", "result": { ... }, "timeMs": 0.21 }
{ "ok": false, "op": "roots.find", "error": { "code": "INVALID_PARAM", "message": "'a' must be a finite number", "param": "a" } }
```

Error codes: `PARSE_ERROR` (bad JSON), `INVALID_REQUEST`, `UNKNOWN_OP`,
`INVALID_PARAM` (with `param`), `EXPRESSION_ERROR` (with `position`),
`NUMERIC_ERROR` (e.g. singular matrix), `LIMIT_EXCEEDED`, `INTERNAL`.
Numerical *failure* (no convergence) is **not** an error: the result carries
`converged: false` and a `message`.

Numbers may be given as JSON numbers or as strings with constants
(`"pi/2"`, `"-inf"`, `"2e-3"`). Non-finite results are serialised as `null`;
where a textual form matters (`expression.evaluate`) a `text` field is included.

Transports: `cml_execute()` (C ABI / WASM), `cml-cli '<json>'`,
`cml-cli --stdin` (one request per line), `POST /api/compute` (gateway).
`meta.ops` returns this catalogue with parameter schemas at run time.

## meta
| op | params | result |
|---|---|---|
| `meta.ops` | – | `{ops: [{name, category, description, params}], count}` |
| `meta.version` | – | `{version, limits}` |

## expression
| op | params | result |
|---|---|---|
| `expression.evaluate` | `expr*`, `x`, `y`, `t`, `vars{}` | `{value, text, finite, expression, variables}` |
| `expression.parse` | `expr*` | `{expression, simplified, variables, nodes, isConstant, value?}` |
| `expression.derivative` | `expr*`, `var="x"`, `order=1`, `at` | `{derivative, derivatives[], value?}` |
| `expression.table` | `expr*`, `var`, `from`, `to`, `samples`, `derivative` | `{x[], y[], dy?[], derivative?}` |
| `expression.functions` | – | `{functions[], constants[]}` |

Expression grammar: `+ - * / ^ %`, `**`, unary minus, postfix `!`, implicit
multiplication (`2x`, `3sin(x)`, `(x+1)(x-1)`), `sin x` for one-argument
functions, constants `pi e tau phi inf nan eulergamma`, ~60 functions
(`sin … acoth, exp exp2 expm1 ln log log2 log10 log1p sqrt cbrt abs floor ceil
round trunc sign sq gamma lgamma digamma erf erfc fact deg rad isprime fib
normcdf normpdf pow atan2 hypot min max mod logb root beta ncr npr gcd lcm`),
`log(b, x)` for log base b.

## core / special
| op | params | result |
|---|---|---|
| `core.evaluate` | `fn*`, `x*` | `{value, text}` |
| `core.compare` | `fn*`, `from`, `to`, `samples` | `{rows[{x,cml,libm,ulps}], worstUlps, worstAt, meanUlps, hasReference}` |
| `core.constants` | – | `{pi, tau, e, phi, …}` |
| `core.functions` | – | `{functions[]}` |
| `special.numberTheory` | `fn*` ∈ factorial, binomial, permutations, gcd, lcm, isPrime, primeFactors, primes, fibonacci, beta; `n`, `k`, `a`, `b` | `{value}` or `{factors[]}` / `{primes[], count}` (+ `exact` string for big integers) |

## roots
| op | params | result |
|---|---|---|
| `roots.find` | `f*`, `method` (bisection, regula_falsi, secant, newton, halley, brent, ridders, steffensen, fixed_point, linear_search, variable_step), `a`, `b`, `x0`, `x1`, `df`, `d2f`, `step`, `tol`, `maxIter`, `history`, `maxHistory` | `{root, fRoot, iterations, evaluations, converged, message, history[{iteration,a,b,x,fx,error}], derivative?}` |
| `roots.compare` | `f*`, `a`, `b`, `x0`, `tol`, `maxIter` | `{results[]}` |
| `roots.all` | `f*`, `a*`, `b*`, `samples`, `tol` | `{roots[{root,fRoot,iterations,converged}], count}` |

## polynomial
Polynomials are given as text (`"3x^2 - 2x + 1"`) or as coefficient arrays, lowest degree first.

| op | params | result |
|---|---|---|
| `polynomial.parse` | `p*` | `{coefficients, degree, text, unicode}` |
| `polynomial.evaluate` | `p*`, `x`, `xs[]` | `+ {value, derivativeValue}` / `{values[]}` |
| `polynomial.roots` | `p*`, `tol`, `maxIter` | `+ {roots[{re,im,residual}], realRoots[], iterations, converged}` |
| `polynomial.calculus` | `p*`, `fn` (derivative, integral, definite_integral), `order`, `constant`, `a`, `b` | `{input, result, value?}` |
| `polynomial.arithmetic` | `a*`, `b`, `fn` (add, subtract, multiply, divide, modulo, gcd, compose, power, scale), `n`, `k` | `{a, b?, result, quotient?, remainder?}` |
| `polynomial.table` | `p*`, `from`, `to`, `samples` | `{x[], y[], polynomial}` |
| `polynomial.fromRoots` | `roots*[]`, `leading` | polynomial |

## matrix
| op | params | result |
|---|---|---|
| `matrix.compute` | `A*` (rows or `"1 2; 3 4"`), `B`, `b[]`, `fn` (determinant, inverse, transpose, rank, rref, lu, qr, cholesky, eigen, solve, least_squares, multiply, add, subtract, hadamard, trace, norms, power, cofactor, adjugate, condition, scale, info, power_iteration), `n`, `k`, `tol`, `strassen` | depends on `fn`: `value`, `matrix`, `L U P permutation sign`, `Q R`, `values imag vectors`, `x residual`, `rank pivots`, … |

Limits: 400×400 matrices; cofactor/adjugate 12×12.

## optimize
| op | params | result |
|---|---|---|
| `optimize.minimize` | `f*`, `method` (golden, fibonacci, brent, newton, binary, linear_search, variable_step), `a`, `b`, `x0`, `step`, `tol`, `maxIter`, `maximize` | `{x, fx, iterations, evaluations, converged, message, history[]}` |
| `optimize.bracket` | `f*`, `x0`, `step`, `maxIter` | `{a,b,c,fa,fb,fc,found}` |
| `optimize.minimizeNd` | `f*`, `method` (nelder_mead, gradient_descent), `vars[]`, `x0*[]`, `step`, `learningRate`, `tol`, `maxIter`, `maximize`, `path` | `{x[], fx, iterations, evaluations, converged, path[][], pathFx[]}` |
| `optimize.surface` | `f*`, `vars[2]`, `xFrom xTo yFrom yTo`, `xSamples ySamples` | `{x[], y[], z[][], zMin, zMax}` |

## integrate
| op | params | result |
|---|---|---|
| `integrate.compute` | `f*`, `a*`, `b*` (may be ±inf with method auto), `method` (midpoint, trapezoid, simpson, simpson38, boole, romberg, gauss_legendre, adaptive_simpson, monte_carlo, auto), `n`, `points`, `tol`, `maxLevels`, `samples`, `seed` | `{value, errorEstimate, evaluations, subdivisions, converged, message, table?}` |
| `integrate.compare` | `f*`, `a*`, `b*`, `n`, `points`, `samples` | `{reference, results[+absError]}` |
| `integrate.convergence` | `f*`, `a*`, `b*`, `method`, `ns[]` | `{reference, rows[{n,value,absError}]}` |

## ode
| op | params | result |
|---|---|---|
| `ode.solve` | `f` (scalar, in `t`,`y`) or `equations[]` + `state[]`, `y0*[]`, `t0`, `t1`, `h`, `method` (euler, heun, midpoint, rk4, rk45), `tol`, `maxSteps`, `maxPoints` | `{state[], equations[], t[], y[state][point], final[], steps, rejected, evaluations, success, message, thinned}` |

## interpolate
| op | params | result |
|---|---|---|
| `interpolate.compute` | `xs[]`+`ys[]` or `points[[x,y]]`, `method` (lagrange, newton, linear, spline, spline_clamped, polyfit), `degree`, `x`, `xq[]`, `from`, `to`, `samples`, `d0`, `dn` | `{polynomial?, coefficients?, table?, segments?, rSquared?, rmse?, residuals?, value?, values?, curve{x,y}}` |

## stats
| op | params | result |
|---|---|---|
| `stats.describe` | `data*` (array or text), `bins` | `{count, mean, median, mode, min, max, variance, stddev, q1, q3, iqr, skewness, kurtosis, geometricMean, harmonicMean, …, histogram{edges,counts,density}, quantiles{}, sorted?}` |
| `stats.regression` | `x*[]`, `y*[]`, `degree` | `{slope, intercept, r, rSquared, stdErrorSlope, stdErrorIntercept, rmse, residuals[], equation, pearson, spearman, covariance, polynomial?}` |
| `stats.transform` | `data*`, `fn` (zscore, moving_average, cumsum, normalize, ranks, sort), `window` | `{result[]}` |

## Resource limits (meta.version → limits)

| limit | value |
|---|---|
| iterations | 100 000 |
| table samples | 200 000 |
| fixed-rule subdivisions | 2 000 000 |
| ODE steps | 200 000 |
| matrix dimension | 400 |
| polynomial degree | 300 |
| data points | 500 000 |
| request size | 4 MB |
