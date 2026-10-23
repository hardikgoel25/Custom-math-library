// cml/roots.cpp — scalar root finding.
#include "cml/roots.hpp"
#include "cml/core.hpp"
#include "cml/special.hpp"

#include <stdexcept>

namespace cml {

namespace {

struct Counter {
    const RealFn& f;
    int calls = 0;
    explicit Counter(const RealFn& fn) : f(fn) {}
    double operator()(double x) {
        ++calls;
        return f(x);
    }
};

void record(RootResult& r, const RootOptions& opt, int it, double a, double b, double x, double fx, double err) {
    if (!opt.record_history) return;
    if (static_cast<int>(r.history.size()) >= opt.max_history) return;
    r.history.push_back({it, a, b, x, fx, err});
}

void finish(RootResult& r, Counter& c, double x, double fx, bool ok, const std::string& msg) {
    r.root = x;
    r.f_root = fx;
    r.evaluations = c.calls;
    r.converged = ok;
    r.message = msg;
}

void validate(const RootOptions& opt) {
    if (!(opt.tol > 0.0)) throw std::invalid_argument("tol must be positive");
    if (opt.max_iter < 1) throw std::invalid_argument("max_iter must be >= 1");
}

bool bad(double v) { return !is_finite(v); }

} // namespace

// ---------------------------------------------------------------------------
RootResult bisection(const RealFn& f, double a, double b, const RootOptions& opt) {
    validate(opt);
    RootResult r;
    r.method = "bisection";
    Counter c(f);
    if (a > b) { const double t = a; a = b; b = t; }
    double fa = c(a), fb = c(b);
    if (bad(fa) || bad(fb)) { finish(r, c, a, fa, false, "function is not finite at the bracket ends"); return r; }
    if (fa == 0.0) { finish(r, c, a, fa, true, "exact root at left endpoint"); return r; }
    if (fb == 0.0) { finish(r, c, b, fb, true, "exact root at right endpoint"); return r; }
    if (sign(fa) == sign(fb)) { finish(r, c, a, fa, false, "f(a) and f(b) must have opposite signs"); return r; }
    double m = a, fm = fa;
    for (int it = 1; it <= opt.max_iter; ++it) {
        m = a + 0.5 * (b - a);
        fm = c(m);
        r.iterations = it;
        record(r, opt, it, a, b, m, fm, 0.5 * (b - a));
        if (bad(fm)) { finish(r, c, m, fm, false, "function became non-finite"); return r; }
        if (fm == 0.0 || 0.5 * (b - a) <= opt.tol) {
            finish(r, c, m, fm, true, "converged: bracket width below tolerance");
            return r;
        }
        if (sign(fm) == sign(fa)) { a = m; fa = fm; } else { b = m; fb = fm; }
    }
    finish(r, c, m, fm, false, "maximum iterations reached");
    return r;
}

// ---------------------------------------------------------------------------
RootResult regula_falsi(const RealFn& f, double a, double b, const RootOptions& opt) {
    validate(opt);
    RootResult r;
    r.method = "regula_falsi";
    Counter c(f);
    if (a > b) { const double t = a; a = b; b = t; }
    double fa = c(a), fb = c(b);
    if (bad(fa) || bad(fb)) { finish(r, c, a, fa, false, "function is not finite at the bracket ends"); return r; }
    if (fa == 0.0) { finish(r, c, a, fa, true, "exact root at left endpoint"); return r; }
    if (fb == 0.0) { finish(r, c, b, fb, true, "exact root at right endpoint"); return r; }
    if (sign(fa) == sign(fb)) { finish(r, c, a, fa, false, "f(a) and f(b) must have opposite signs"); return r; }
    int side = 0;
    double x = a, fx = fa, prev = a;
    for (int it = 1; it <= opt.max_iter; ++it) {
        const double denom = fb - fa;
        if (denom == 0.0) { finish(r, c, x, fx, false, "degenerate secant (f(a) == f(b))"); return r; }
        x = (a * fb - b * fa) / denom;
        fx = c(x);
        r.iterations = it;
        record(r, opt, it, a, b, x, fx, abs(x - prev));
        if (bad(fx)) { finish(r, c, x, fx, false, "function became non-finite"); return r; }
        if (fx == 0.0 || abs(fx) < opt.tol * 1e-3 || abs(x - prev) <= opt.tol || (b - a) <= opt.tol) {
            finish(r, c, x, fx, true, "converged");
            return r;
        }
        if (sign(fx) == sign(fa)) {
            a = x; fa = fx;
            if (side == -1) fb *= 0.5;   // Illinois modification keeps both ends moving
            side = -1;
        } else {
            b = x; fb = fx;
            if (side == 1) fa *= 0.5;
            side = 1;
        }
        prev = x;
    }
    finish(r, c, x, fx, false, "maximum iterations reached");
    return r;
}

// ---------------------------------------------------------------------------
RootResult secant(const RealFn& f, double x0, double x1, const RootOptions& opt) {
    validate(opt);
    RootResult r;
    r.method = "secant";
    Counter c(f);
    double f0 = c(x0), f1 = c(x1);
    if (bad(f0) || bad(f1)) { finish(r, c, x1, f1, false, "function is not finite at the starting points"); return r; }
    if (f0 == 0.0) { finish(r, c, x0, f0, true, "exact root at x0"); return r; }
    if (f1 == 0.0) { finish(r, c, x1, f1, true, "exact root at x1"); return r; }
    for (int it = 1; it <= opt.max_iter; ++it) {
        const double denom = f1 - f0;
        if (denom == 0.0) { finish(r, c, x1, f1, false, "stagnation: f(x_k) == f(x_{k-1})"); return r; }
        const double x2 = x1 - f1 * (x1 - x0) / denom;
        const double f2 = c(x2);
        r.iterations = it;
        record(r, opt, it, x0, x1, x2, f2, abs(x2 - x1));
        if (bad(x2) || bad(f2)) { finish(r, c, x2, f2, false, "iteration diverged"); return r; }
        x0 = x1; f0 = f1; x1 = x2; f1 = f2;
        if (f2 == 0.0 || abs(x1 - x0) <= opt.tol) { finish(r, c, x1, f1, true, "converged"); return r; }
    }
    finish(r, c, x1, f1, false, "maximum iterations reached");
    return r;
}

// ---------------------------------------------------------------------------
RootResult newton(const RealFn& f, const RealFn& df, double x0, const RootOptions& opt) {
    validate(opt);
    RootResult r;
    r.method = "newton";
    Counter c(f);
    double x = x0, fx = c(x);
    if (bad(fx)) { finish(r, c, x, fx, false, "function is not finite at x0"); return r; }
    if (fx == 0.0) { finish(r, c, x, fx, true, "exact root at x0"); return r; }
    for (int it = 1; it <= opt.max_iter; ++it) {
        const double d = df(x);
        ++c.calls;
        if (bad(d)) { finish(r, c, x, fx, false, "derivative is not finite"); return r; }
        if (d == 0.0) { finish(r, c, x, fx, false, "zero derivative; Newton step undefined"); return r; }
        const double step = fx / d;
        const double xn = x - step;
        const double fn = c(xn);
        r.iterations = it;
        record(r, opt, it, x, xn, xn, fn, abs(step));
        if (bad(xn) || bad(fn)) { finish(r, c, xn, fn, false, "iteration diverged"); return r; }
        x = xn; fx = fn;
        if (fx == 0.0 || abs(step) <= opt.tol) { finish(r, c, x, fx, true, "converged"); return r; }
    }
    finish(r, c, x, fx, false, "maximum iterations reached");
    return r;
}

RootResult newton(const RealFn& f, double x0, const RootOptions& opt) {
    RealFn df = [&f](double x) {
        const double h = 1e-6 * fmax(1.0, abs(x));
        return derivative(f, x, h);
    };
    RootResult r = newton(f, df, x0, opt);
    r.method = "newton_numeric";
    return r;
}

// ---------------------------------------------------------------------------
RootResult halley(const RealFn& f, const RealFn& df, const RealFn& d2f, double x0, const RootOptions& opt) {
    validate(opt);
    RootResult r;
    r.method = "halley";
    Counter c(f);
    double x = x0, fx = c(x);
    if (bad(fx)) { finish(r, c, x, fx, false, "function is not finite at x0"); return r; }
    if (fx == 0.0) { finish(r, c, x, fx, true, "exact root at x0"); return r; }
    for (int it = 1; it <= opt.max_iter; ++it) {
        const double d1 = df(x), d2 = d2f(x);
        c.calls += 2;
        const double denom = 2.0 * d1 * d1 - fx * d2;
        if (bad(d1) || bad(d2) || denom == 0.0) { finish(r, c, x, fx, false, "degenerate Halley step"); return r; }
        const double step = 2.0 * fx * d1 / denom;
        const double xn = x - step;
        const double fn = c(xn);
        r.iterations = it;
        record(r, opt, it, x, xn, xn, fn, abs(step));
        if (bad(xn) || bad(fn)) { finish(r, c, xn, fn, false, "iteration diverged"); return r; }
        x = xn; fx = fn;
        if (fx == 0.0 || abs(step) <= opt.tol) { finish(r, c, x, fx, true, "converged"); return r; }
    }
    finish(r, c, x, fx, false, "maximum iterations reached");
    return r;
}

// ---------------------------------------------------------------------------
// Brent's method (van Wijngaarden-Dekker-Brent): inverse quadratic
// interpolation and secant steps with a bisection safeguard; guaranteed to
// converge and typically superlinear.
RootResult brent(const RealFn& f, double a, double b, const RootOptions& opt) {
    validate(opt);
    RootResult r;
    r.method = "brent";
    Counter c(f);
    double fa = c(a), fb = c(b);
    if (bad(fa) || bad(fb)) { finish(r, c, a, fa, false, "function is not finite at the bracket ends"); return r; }
    if (fa == 0.0) { finish(r, c, a, fa, true, "exact root at left endpoint"); return r; }
    if (fb == 0.0) { finish(r, c, b, fb, true, "exact root at right endpoint"); return r; }
    if (sign(fa) == sign(fb)) { finish(r, c, a, fa, false, "f(a) and f(b) must have opposite signs"); return r; }
    double cc = b, fc = fb, d = 0.0, e = 0.0;
    for (int it = 1; it <= opt.max_iter; ++it) {
        if (sign(fb) == sign(fc)) { cc = a; fc = fa; e = d = b - a; }
        if (abs(fc) < abs(fb)) { a = b; b = cc; cc = a; fa = fb; fb = fc; fc = fa; }
        const double tol1 = 2.0 * constants::epsilon * abs(b) + 0.5 * opt.tol;
        const double xm = 0.5 * (cc - b);
        r.iterations = it;
        record(r, opt, it, fmin(b, cc), fmax(b, cc), b, fb, abs(xm));
        if (abs(xm) <= tol1 || fb == 0.0) { finish(r, c, b, fb, true, "converged"); return r; }
        if (abs(e) >= tol1 && abs(fa) > abs(fb)) {
            const double s = fb / fa;
            double p, q;
            if (a == cc) {
                p = 2.0 * xm * s;
                q = 1.0 - s;
            } else {
                const double qq = fa / fc, rr = fb / fc;
                p = s * (2.0 * xm * qq * (qq - rr) - (b - a) * (rr - 1.0));
                q = (qq - 1.0) * (rr - 1.0) * (s - 1.0);
            }
            if (p > 0.0) q = -q;
            p = abs(p);
            const double min1 = 3.0 * xm * q - abs(tol1 * q);
            const double min2 = abs(e * q);
            if (2.0 * p < fmin(min1, min2)) { e = d; d = p / q; }
            else { d = xm; e = d; }
        } else {
            d = xm; e = d;
        }
        a = b; fa = fb;
        if (abs(d) > tol1) b += d;
        else b += copysign(tol1, xm);
        fb = c(b);
        if (bad(fb)) { finish(r, c, b, fb, false, "function became non-finite"); return r; }
    }
    finish(r, c, b, fb, false, "maximum iterations reached");
    return r;
}

// ---------------------------------------------------------------------------
RootResult ridders(const RealFn& f, double a, double b, const RootOptions& opt) {
    validate(opt);
    RootResult r;
    r.method = "ridders";
    Counter c(f);
    if (a > b) { const double t = a; a = b; b = t; }
    double fa = c(a), fb = c(b);
    if (bad(fa) || bad(fb)) { finish(r, c, a, fa, false, "function is not finite at the bracket ends"); return r; }
    if (fa == 0.0) { finish(r, c, a, fa, true, "exact root at left endpoint"); return r; }
    if (fb == 0.0) { finish(r, c, b, fb, true, "exact root at right endpoint"); return r; }
    if (sign(fa) == sign(fb)) { finish(r, c, a, fa, false, "f(a) and f(b) must have opposite signs"); return r; }
    double x = a, fx = fa, prev = a;
    for (int it = 1; it <= opt.max_iter; ++it) {
        const double m = 0.5 * (a + b);
        const double fm = c(m);
        const double s = sqrt(fm * fm - fa * fb);
        if (s == 0.0) { finish(r, c, m, fm, fm == 0.0, fm == 0.0 ? "converged" : "degenerate step"); return r; }
        x = m + (m - a) * ((fa >= fb ? 1.0 : -1.0) * fm / s);
        fx = c(x);
        r.iterations = it;
        record(r, opt, it, a, b, x, fx, abs(x - prev));
        if (bad(fx)) { finish(r, c, x, fx, false, "function became non-finite"); return r; }
        if (fx == 0.0 || abs(x - prev) <= opt.tol) { finish(r, c, x, fx, true, "converged"); return r; }
        prev = x;
        if (sign(fm) != sign(fx)) { a = m; fa = fm; b = x; fb = fx; }
        else if (sign(fa) != sign(fx)) { b = x; fb = fx; }
        else { a = x; fa = fx; }
        if (abs(b - a) <= opt.tol) { finish(r, c, x, fx, true, "converged: bracket width below tolerance"); return r; }
    }
    finish(r, c, x, fx, false, "maximum iterations reached");
    return r;
}

// ---------------------------------------------------------------------------
RootResult steffensen(const RealFn& f, double x0, const RootOptions& opt) {
    validate(opt);
    RootResult r;
    r.method = "steffensen";
    Counter c(f);
    double x = x0, fx = c(x);
    if (bad(fx)) { finish(r, c, x, fx, false, "function is not finite at x0"); return r; }
    for (int it = 1; it <= opt.max_iter; ++it) {
        if (fx == 0.0) { finish(r, c, x, fx, true, "converged"); return r; }
        const double g = (c(x + fx) - fx) / fx;
        if (bad(g) || g == 0.0) { finish(r, c, x, fx, false, "degenerate Steffensen step"); return r; }
        const double xn = x - fx / g;
        const double fn = c(xn);
        r.iterations = it;
        record(r, opt, it, x, xn, xn, fn, abs(xn - x));
        if (bad(xn) || bad(fn)) { finish(r, c, xn, fn, false, "iteration diverged"); return r; }
        const double step = abs(xn - x);
        x = xn; fx = fn;
        if (step <= opt.tol) { finish(r, c, x, fx, true, "converged"); return r; }
    }
    finish(r, c, x, fx, false, "maximum iterations reached");
    return r;
}

RootResult fixed_point(const RealFn& g, double x0, const RootOptions& opt) {
    validate(opt);
    RootResult r;
    r.method = "fixed_point";
    Counter c(g);
    double x = x0;
    for (int it = 1; it <= opt.max_iter; ++it) {
        const double xn = c(x);
        r.iterations = it;
        record(r, opt, it, x, xn, xn, xn - x, abs(xn - x));
        if (bad(xn)) { finish(r, c, xn, nan(), false, "iteration diverged"); return r; }
        const double step = abs(xn - x);
        x = xn;
        if (step <= opt.tol) { finish(r, c, x, 0.0, true, "converged"); r.f_root = c(x) - x; return r; }
    }
    finish(r, c, x, c(x) - x, false, "maximum iterations reached");
    return r;
}

// ---------------------------------------------------------------------------
RootResult linear_search(const RealFn& f, double a, double b, double step, const RootOptions& opt) {
    validate(opt);
    if (!(step > 0.0)) throw std::invalid_argument("step must be positive");
    RootResult r;
    r.method = "linear_search";
    Counter c(f);
    if (a > b) { const double t = a; a = b; b = t; }
    const double span = b - a;
    if (span / step > 5e6) throw std::invalid_argument("step too small for interval (more than 5e6 samples)");
    double x = a, fx = c(x);
    if (fx == 0.0) { finish(r, c, x, fx, true, "exact root at left endpoint"); return r; }
    int it = 0;
    while (x < b) {
        double y = x + step;
        if (y > b) y = b;
        const double fy = c(y);
        ++it;
        if (it > opt.max_history && opt.record_history) { /* keep history bounded */ }
        record(r, opt, it, x, y, y, fy, step);
        if (bad(fy)) { x = y; fx = fy; continue; }
        if (fy == 0.0) { r.iterations = it; finish(r, c, y, fy, true, "exact root found"); return r; }
        if (sign(fx) != sign(fy) && !bad(fx)) {
            // refine inside [x, y] with bisection
            RootOptions inner = opt;
            inner.record_history = false;
            const RootResult fine = bisection(f, x, y, inner);
            r.iterations = it;
            finish(r, c, fine.root, fine.f_root, fine.converged, "sign change found and refined");
            r.evaluations += fine.evaluations;
            return r;
        }
        x = y; fx = fy;
        if (y >= b) break;
    }
    r.iterations = it;
    finish(r, c, x, fx, false, "no sign change found in interval");
    return r;
}

RootResult variable_step(const RealFn& f, double a, double b, double initial_step, const RootOptions& opt) {
    validate(opt);
    if (!(initial_step > 0.0)) throw std::invalid_argument("initial_step must be positive");
    RootResult r;
    r.method = "variable_step";
    Counter c(f);
    if (a > b) { const double t = a; a = b; b = t; }
    double x = a, fx = c(x), h = initial_step;
    if (fx == 0.0) { finish(r, c, x, fx, true, "exact root at left endpoint"); return r; }
    for (int it = 1; it <= opt.max_iter * 50; ++it) {
        double y = x + h;
        if (y > b) y = b;
        const double fy = c(y);
        record(r, opt, it, x, y, y, fy, h);
        r.iterations = it;
        if (bad(fy)) { finish(r, c, y, fy, false, "function became non-finite"); return r; }
        if (fy == 0.0) { finish(r, c, y, fy, true, "exact root found"); return r; }
        if (sign(fx) != sign(fy)) {
            if (h <= opt.tol) { finish(r, c, 0.5 * (x + y), c(0.5 * (x + y)), true, "converged: step below tolerance"); return r; }
            h *= 0.5;                       // sign change: shrink the step, stay at x
            continue;
        }
        x = y; fx = fy;
        h *= 2.0;                           // same sign: accelerate
        if (x >= b) { finish(r, c, x, fx, false, "reached end of interval without a sign change"); return r; }
    }
    finish(r, c, x, fx, false, "maximum iterations reached");
    return r;
}

// ---------------------------------------------------------------------------
std::vector<RootResult> find_all_roots(const RealFn& f, double a, double b, int samples, const RootOptions& opt) {
    validate(opt);
    if (samples < 2) throw std::invalid_argument("samples must be >= 2");
    if (samples > 1000000) throw std::invalid_argument("samples must be <= 1e6");
    if (a > b) { const double t = a; a = b; b = t; }
    std::vector<RootResult> out;
    const double h = (b - a) / samples;
    double x0 = a, f0 = f(x0);
    for (int i = 1; i <= samples; ++i) {
        const double x1 = (i == samples) ? b : a + i * h;
        const double f1 = f(x1);
        if (is_finite(f0) && is_finite(f1)) {
            if (f0 == 0.0 && (out.empty() || abs(out.back().root - x0) > h)) {
                RootResult r;
                r.method = "scan";
                r.root = x0; r.f_root = 0.0; r.converged = true; r.message = "exact zero at sample";
                out.push_back(r);
            } else if (sign(f0) != sign(f1) && f0 != 0.0 && f1 != 0.0) {
                RootResult r = brent(f, x0, x1, opt);
                // Reject sign changes that are poles (|f| large at the "root").
                if (r.converged && abs(r.f_root) < 1e-3 * fmax(1.0, fmax(abs(f0), abs(f1))))
                    out.push_back(r);
            }
        }
        x0 = x1; f0 = f1;
    }
    return out;
}

} // namespace cml
