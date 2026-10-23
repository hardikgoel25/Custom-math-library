// cml/optimize.cpp — minimisation routines.
#include "cml/optimize.hpp"
#include "cml/core.hpp"
#include "cml/special.hpp"

#include <algorithm>
#include <stdexcept>

namespace cml {

namespace {

struct Counter {
    const RealFn& f;
    int calls = 0;
    explicit Counter(const RealFn& fn) : f(fn) {}
    double operator()(double x) { ++calls; return f(x); }
};

void record(MinResult& r, const MinOptions& opt, int it, double a, double b, double x, double fx) {
    if (!opt.record_history || static_cast<int>(r.history.size()) >= opt.max_history) return;
    r.history.push_back({it, a, b, x, fx});
}
void finish(MinResult& r, Counter& c, double x, double fx, bool ok, const std::string& msg) {
    r.x = x; r.fx = fx; r.evaluations = c.calls; r.converged = ok; r.message = msg;
}
void validate(const MinOptions& opt) {
    if (!(opt.tol > 0.0)) throw std::invalid_argument("tol must be positive");
    if (opt.max_iter < 1) throw std::invalid_argument("max_iter must be >= 1");
}
bool bad(double v) { return !is_finite(v); }

} // namespace

Bracket bracket_minimum(const RealFn& f, double x0, double step, int max_iter) {
    if (!(step > 0.0)) throw std::invalid_argument("step must be positive");
    Bracket br;
    double a = x0, b = x0 + step;
    double fa = f(a), fb = f(b);
    br.evaluations = 2;
    if (fb > fa) { std::swap(a, b); std::swap(fa, fb); step = -step; }
    double c = b + step, fc = f(c);
    ++br.evaluations;
    for (int it = 0; it < max_iter && fc < fb; ++it) {
        a = b; fa = fb;
        b = c; fb = fc;
        step *= 2.0;
        c = b + step;
        fc = f(c);
        ++br.evaluations;
        if (bad(fc)) break;
    }
    if (a > c) { std::swap(a, c); std::swap(fa, fc); }
    br.a = a; br.b = b; br.c = c; br.fa = fa; br.fb = fb; br.fc = fc;
    br.found = (fb <= fa && fb <= fc) && is_finite(fc);
    return br;
}

// ---------------------------------------------------------------------------
MinResult golden_section(const RealFn& f, double a, double b, const MinOptions& opt) {
    validate(opt);
    MinResult r;
    r.method = "golden_section";
    Counter c(f);
    if (a > b) std::swap(a, b);
    const double invphi = constants::phi - 1.0;         // 0.618...
    double x1 = b - invphi * (b - a), x2 = a + invphi * (b - a);
    double f1 = c(x1), f2 = c(x2);
    for (int it = 1; it <= opt.max_iter; ++it) {
        r.iterations = it;
        if (bad(f1) || bad(f2)) { finish(r, c, x1, f1, false, "function became non-finite"); return r; }
        if (f1 < f2) { b = x2; x2 = x1; f2 = f1; x1 = b - invphi * (b - a); f1 = c(x1); }
        else         { a = x1; x1 = x2; f1 = f2; x2 = a + invphi * (b - a); f2 = c(x2); }
        const double xb = f1 < f2 ? x1 : x2;
        record(r, opt, it, a, b, xb, fmin(f1, f2));
        if (abs(b - a) <= opt.tol * fmax(1.0, abs(xb))) {
            const double xm = 0.5 * (a + b);
            finish(r, c, xm, c(xm), true, "converged: bracket width below tolerance");
            return r;
        }
    }
    const double xm = 0.5 * (a + b);
    finish(r, c, xm, c(xm), false, "maximum iterations reached");
    return r;
}

MinResult fibonacci_search(const RealFn& f, double a, double b, const MinOptions& opt) {
    validate(opt);
    MinResult r;
    r.method = "fibonacci";
    Counter c(f);
    if (a > b) std::swap(a, b);
    // Choose n such that F_n > (b - a) / tol  (iteratively computed Fibonacci numbers)
    std::vector<double> F = {1.0, 1.0};
    const double target = (b - a) / opt.tol;
    while (F.back() < target && F.size() < 200) F.push_back(F[F.size() - 1] + F[F.size() - 2]);
    const int n = static_cast<int>(F.size()) - 1;
    double x1 = a + (F[n - 2] / F[n]) * (b - a);
    double x2 = a + (F[n - 1] / F[n]) * (b - a);
    double f1 = c(x1), f2 = c(x2);
    int it = 0;
    for (int k = 1; k <= n - 2 && it < opt.max_iter; ++k) {
        ++it;
        r.iterations = it;
        if (bad(f1) || bad(f2)) { finish(r, c, x1, f1, false, "function became non-finite"); return r; }
        if (f1 > f2) {
            a = x1; x1 = x2; f1 = f2;
            x2 = a + (F[n - k - 1] / F[n - k]) * (b - a);
            f2 = c(x2);
        } else {
            b = x2; x2 = x1; f2 = f1;
            x1 = a + (F[n - k - 2] / F[n - k]) * (b - a);
            f1 = c(x1);
        }
        record(r, opt, it, a, b, f1 < f2 ? x1 : x2, fmin(f1, f2));
        if (abs(b - a) <= opt.tol) break;
    }
    const double xm = 0.5 * (a + b);
    finish(r, c, xm, c(xm), abs(b - a) <= opt.tol * 2.0, "converged: bracket width below tolerance");
    if (!r.converged) r.message = "maximum iterations reached";
    return r;
}

// Brent's parabolic-interpolation minimiser (after Numerical Recipes / Brent 1973).
MinResult brent_minimize(const RealFn& f, double a, double b, const MinOptions& opt) {
    validate(opt);
    MinResult r;
    r.method = "brent";
    Counter c(f);
    if (a > b) std::swap(a, b);
    const double cgold = 0.3819660112501051;
    double x = a + cgold * (b - a), w = x, v = x;
    double fx = c(x), fw = fx, fv = fx;
    double d = 0.0, e = 0.0;
    for (int it = 1; it <= opt.max_iter; ++it) {
        r.iterations = it;
        const double xm = 0.5 * (a + b);
        const double tol1 = opt.tol * abs(x) + 1e-10, tol2 = 2.0 * tol1;
        record(r, opt, it, a, b, x, fx);
        if (abs(x - xm) <= tol2 - 0.5 * (b - a)) { finish(r, c, x, fx, true, "converged"); return r; }
        if (abs(e) > tol1) {
            double rr = (x - w) * (fx - fv);
            double q = (x - v) * (fx - fw);
            double p = (x - v) * q - (x - w) * rr;
            q = 2.0 * (q - rr);
            if (q > 0.0) p = -p;
            q = abs(q);
            const double etemp = e;
            e = d;
            if (abs(p) >= abs(0.5 * q * etemp) || p <= q * (a - x) || p >= q * (b - x)) {
                e = (x >= xm) ? a - x : b - x;
                d = cgold * e;
            } else {
                d = p / q;
                const double u = x + d;
                if (u - a < tol2 || b - u < tol2) d = copysign(tol1, xm - x);
            }
        } else {
            e = (x >= xm) ? a - x : b - x;
            d = cgold * e;
        }
        const double u = (abs(d) >= tol1) ? x + d : x + copysign(tol1, d);
        const double fu = c(u);
        if (bad(fu)) { finish(r, c, x, fx, false, "function became non-finite"); return r; }
        if (fu <= fx) {
            if (u >= x) a = x; else b = x;
            v = w; fv = fw; w = x; fw = fx; x = u; fx = fu;
        } else {
            if (u < x) a = u; else b = u;
            if (fu <= fw || w == x) { v = w; fv = fw; w = u; fw = fu; }
            else if (fu <= fv || v == x || v == w) { v = u; fv = fu; }
        }
    }
    finish(r, c, x, fx, false, "maximum iterations reached");
    return r;
}

MinResult newton_minimize(const RealFn& f, const RealFn& df, const RealFn& d2f, double x0, const MinOptions& opt) {
    validate(opt);
    MinResult r;
    r.method = "newton";
    Counter c(f);
    double x = x0;
    double fx = c(x);
    for (int it = 1; it <= opt.max_iter; ++it) {
        const double g = df(x), h = d2f(x);
        c.calls += 2;
        if (bad(g) || bad(h)) { finish(r, c, x, fx, false, "derivative is not finite"); return r; }
        if (abs(h) < 1e-300) { finish(r, c, x, fx, false, "second derivative is zero"); return r; }
        const double step = g / h;
        const double xn = x - step;
        const double fn = c(xn);
        r.iterations = it;
        record(r, opt, it, x, xn, xn, fn);
        if (bad(xn) || bad(fn)) { finish(r, c, xn, fn, false, "iteration diverged"); return r; }
        x = xn; fx = fn;
        if (abs(step) <= opt.tol) {
            const bool is_min = h > 0.0;
            finish(r, c, x, fx, is_min, is_min ? "converged to a local minimum" : "converged to a stationary point that is not a minimum (f'' <= 0)");
            return r;
        }
    }
    finish(r, c, x, fx, false, "maximum iterations reached");
    return r;
}

MinResult newton_minimize(const RealFn& f, double x0, const MinOptions& opt) {
    RealFn df = [&f](double x) { return derivative(f, x, 1e-5 * fmax(1.0, abs(x))); };
    RealFn d2f = [&f](double x) { return second_derivative(f, x, 1e-4 * fmax(1.0, abs(x))); };
    MinResult r = newton_minimize(f, df, d2f, x0, opt);
    r.method = "newton_numeric";
    return r;
}

MinResult binary_search_min(const RealFn& f, double a, double b, const MinOptions& opt) {
    validate(opt);
    MinResult r;
    r.method = "binary";
    Counter c(f);
    if (a > b) std::swap(a, b);
    double m = a;
    for (int it = 1; it <= opt.max_iter; ++it) {
        m = 0.5 * (a + b);
        const double h = fmax(1e-7, opt.tol * 0.5);
        const double slope = c(m + h) - c(m - h);
        r.iterations = it;
        record(r, opt, it, a, b, m, c(m));
        if (bad(slope)) { finish(r, c, m, c(m), false, "function became non-finite"); return r; }
        if (slope > 0.0) b = m; else a = m;
        if (abs(b - a) <= opt.tol) { finish(r, c, m, c(m), true, "converged: bracket width below tolerance"); return r; }
    }
    finish(r, c, m, c(m), false, "maximum iterations reached");
    return r;
}

MinResult linear_search_min(const RealFn& f, double a, double b, double step, const MinOptions& opt) {
    validate(opt);
    if (!(step > 0.0)) throw std::invalid_argument("step must be positive");
    MinResult r;
    r.method = "linear_search";
    Counter c(f);
    if (a > b) std::swap(a, b);
    if ((b - a) / step > 5e6) throw std::invalid_argument("step too small for interval (more than 5e6 samples)");
    double x = a, fx = c(x);
    int it = 0;
    while (x < b) {
        double y = fmin(x + step, b);
        const double fy = c(y);
        ++it;
        record(r, opt, it, x, y, y, fy);
        if (bad(fy)) { x = y; fx = fy; continue; }
        if (fy > fx) {
            MinOptions inner = opt;
            inner.record_history = false;
            const double lo = fmax(a, x - step);
            const MinResult fine = golden_section(f, lo, y, inner);
            r.iterations = it;
            finish(r, c, fine.x, fine.fx, true, "minimum bracketed and refined");
            r.evaluations += fine.evaluations;
            return r;
        }
        x = y; fx = fy;
        if (y >= b) break;
    }
    r.iterations = it;
    finish(r, c, x, fx, false, "function kept decreasing up to the right endpoint");
    return r;
}

MinResult variable_step_min(const RealFn& f, double a, double b, double initial_step, const MinOptions& opt) {
    validate(opt);
    if (!(initial_step > 0.0)) throw std::invalid_argument("initial_step must be positive");
    MinResult r;
    r.method = "variable_step";
    Counter c(f);
    if (a > b) std::swap(a, b);
    double x = a, fx = c(x), h = initial_step;
    for (int it = 1; it <= opt.max_iter * 20; ++it) {
        double y = clamp(x + h, a, b);
        const double fy = c(y);
        r.iterations = it;
        record(r, opt, it, fmin(x, y), fmax(x, y), x, fx);
        if (bad(fy)) { finish(r, c, x, fx, false, "function became non-finite"); return r; }
        if (fy < fx) {
            x = y; fx = fy;
            h *= 2.0;                               // keep going, faster
            if ((h > 0 && x >= b) || (h < 0 && x <= a)) h = -h * 0.25; // bounced off an edge
        } else {
            h = -h * 0.5;                           // overshot: reverse and shrink
        }
        if (abs(h) <= opt.tol) { finish(r, c, x, fx, true, "converged: step below tolerance"); return r; }
    }
    finish(r, c, x, fx, false, "maximum iterations reached");
    return r;
}

// ---------------------------------------------------------------------------
// Multi-dimensional
// ---------------------------------------------------------------------------
std::vector<double> numeric_gradient(const VecFn& f, const std::vector<double>& x, double h) {
    std::vector<double> g(x.size());
    std::vector<double> xp = x, xm = x;
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double hi = h * fmax(1.0, abs(x[i]));
        xp[i] = x[i] + hi;
        xm[i] = x[i] - hi;
        g[i] = (f(xp) - f(xm)) / (2.0 * hi);
        xp[i] = x[i];
        xm[i] = x[i];
    }
    return g;
}

namespace {
void push_path(MinResultN& r, const MinOptionsN& opt, const std::vector<double>& x, double fx) {
    if (!opt.record_path || static_cast<int>(r.path.size()) >= opt.max_path) return;
    r.path.push_back(x);
    r.path_fx.push_back(fx);
}
} // namespace

MinResultN nelder_mead(const VecFn& f, const std::vector<double>& x0, double initial_step, const MinOptionsN& opt) {
    if (x0.empty()) throw std::invalid_argument("x0 must not be empty");
    if (x0.size() > 64) throw std::invalid_argument("nelder_mead supports up to 64 dimensions");
    if (!(opt.tol > 0.0)) throw std::invalid_argument("tol must be positive");
    const std::size_t n = x0.size();
    MinResultN r;
    r.method = "nelder_mead";
    int evals = 0;
    auto F = [&](const std::vector<double>& p) { ++evals; return f(p); };

    std::vector<std::vector<double>> s(n + 1, x0);
    std::vector<double> fs(n + 1);
    for (std::size_t i = 0; i < n; ++i) s[i + 1][i] += (initial_step != 0.0 ? initial_step : 1.0);
    for (std::size_t i = 0; i <= n; ++i) fs[i] = F(s[i]);

    const double alpha = 1.0, gamma = 2.0, rho = 0.5, sigma = 0.5;
    std::vector<std::size_t> idx(n + 1);
    int it = 0;
    for (; it < opt.max_iter; ++it) {
        for (std::size_t i = 0; i <= n; ++i) idx[i] = i;
        std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) { return fs[a] < fs[b]; });
        std::vector<std::vector<double>> s2(n + 1);
        std::vector<double> fs2(n + 1);
        for (std::size_t i = 0; i <= n; ++i) { s2[i] = s[idx[i]]; fs2[i] = fs[idx[i]]; }
        s = s2; fs = fs2;
        push_path(r, opt, s[0], fs[0]);
        // convergence: size of simplex and spread of values
        double size = 0.0;
        for (std::size_t i = 1; i <= n; ++i)
            for (std::size_t j = 0; j < n; ++j) size = fmax(size, abs(s[i][j] - s[0][j]));
        if (size <= opt.tol * fmax(1.0, abs(s[0][0])) && abs(fs[n] - fs[0]) <= opt.tol * fmax(1.0, abs(fs[0]))) {
            r.converged = true;
            break;
        }
        std::vector<double> centroid(n, 0.0);
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j) centroid[j] += s[i][j] / n;
        std::vector<double> xr(n);
        for (std::size_t j = 0; j < n; ++j) xr[j] = centroid[j] + alpha * (centroid[j] - s[n][j]);
        const double fr = F(xr);
        if (fr < fs[0]) {
            std::vector<double> xe(n);
            for (std::size_t j = 0; j < n; ++j) xe[j] = centroid[j] + gamma * (xr[j] - centroid[j]);
            const double fe = F(xe);
            if (fe < fr) { s[n] = xe; fs[n] = fe; } else { s[n] = xr; fs[n] = fr; }
        } else if (fr < fs[n - 1]) {
            s[n] = xr; fs[n] = fr;
        } else {
            std::vector<double> xc(n);
            const bool outside = fr < fs[n];
            for (std::size_t j = 0; j < n; ++j)
                xc[j] = outside ? centroid[j] + rho * (xr[j] - centroid[j]) : centroid[j] + rho * (s[n][j] - centroid[j]);
            const double fc = F(xc);
            if (fc < (outside ? fr : fs[n])) {
                s[n] = xc; fs[n] = fc;
            } else {
                for (std::size_t i = 1; i <= n; ++i) {
                    for (std::size_t j = 0; j < n; ++j) s[i][j] = s[0][j] + sigma * (s[i][j] - s[0][j]);
                    fs[i] = F(s[i]);
                }
            }
        }
    }
    std::size_t best = 0;
    for (std::size_t i = 1; i <= n; ++i) if (fs[i] < fs[best]) best = i;
    r.x = s[best];
    r.fx = fs[best];
    r.iterations = it;
    r.evaluations = evals;
    r.message = r.converged ? "converged: simplex collapsed below tolerance" : "maximum iterations reached";
    return r;
}

MinResultN gradient_descent(const VecFn& f, const std::function<std::vector<double>(const std::vector<double>&)>& grad,
                            const std::vector<double>& x0, double learning_rate, const MinOptionsN& opt) {
    if (x0.empty()) throw std::invalid_argument("x0 must not be empty");
    if (!(learning_rate > 0.0)) throw std::invalid_argument("learning_rate must be positive");
    MinResultN r;
    r.method = "gradient_descent";
    int evals = 0;
    auto F = [&](const std::vector<double>& p) { ++evals; return f(p); };
    std::vector<double> x = x0;
    double fx = F(x);
    double lr = learning_rate;
    int it = 0;
    for (; it < opt.max_iter; ++it) {
        push_path(r, opt, x, fx);
        const std::vector<double> g = grad(x);
        double gnorm = 0.0;
        for (double v : g) gnorm += v * v;
        gnorm = sqrt(gnorm);
        if (!is_finite(gnorm)) { r.message = "gradient is not finite"; break; }
        if (gnorm <= opt.tol) { r.converged = true; r.message = "converged: gradient norm below tolerance"; break; }
        // Backtracking line search (Armijo) starting from the current learning rate.
        double step = lr;
        std::vector<double> xn(x.size());
        double fn = fx;
        bool accepted = false;
        for (int ls = 0; ls < 40; ++ls) {
            for (std::size_t i = 0; i < x.size(); ++i) xn[i] = x[i] - step * g[i];
            fn = F(xn);
            if (is_finite(fn) && fn <= fx - 1e-4 * step * gnorm * gnorm) { accepted = true; break; }
            step *= 0.5;
        }
        if (!accepted) { r.message = "line search failed to find a decrease"; r.converged = gnorm < 1e-6; break; }
        const double moved = step * gnorm;
        x = xn;
        fx = fn;
        lr = fmin(learning_rate, step * 2.0); // adapt: grow again after successful steps
        if (moved <= opt.tol * fmax(1.0, abs(x[0]))) { r.converged = true; r.message = "converged: step below tolerance"; ++it; break; }
    }
    if (r.message.empty()) r.message = "maximum iterations reached";
    r.x = x;
    r.fx = fx;
    r.iterations = it;
    r.evaluations = evals;
    return r;
}

MinResultN gradient_descent(const VecFn& f, const std::vector<double>& x0, double learning_rate, const MinOptionsN& opt) {
    auto grad = [&f](const std::vector<double>& x) { return numeric_gradient(f, x); };
    return gradient_descent(f, grad, x0, learning_rate, opt);
}

} // namespace cml
