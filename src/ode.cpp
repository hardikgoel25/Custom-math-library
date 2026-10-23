// cml/ode.cpp — Runge–Kutta family solvers.
#include "cml/ode.hpp"
#include "cml/core.hpp"

#include <stdexcept>

namespace cml {

namespace {

using Vec = std::vector<double>;

Vec axpy(const Vec& y, double a, const Vec& k) {
    Vec r(y.size());
    for (std::size_t i = 0; i < y.size(); ++i) r[i] = y[i] + a * k[i];
    return r;
}

bool finite(const Vec& v) {
    for (double x : v) if (!is_finite(x)) return false;
    return true;
}

void validate(const Vec& y0, double h, const OdeOptions& opt, bool need_h) {
    if (y0.empty()) throw std::invalid_argument("y0 must not be empty");
    if (y0.size() > 1000) throw std::invalid_argument("system dimension limit is 1000");
    if (need_h && !(h > 0.0)) throw std::invalid_argument("step size h must be positive");
    if (opt.max_steps < 1 || opt.max_steps > 5000000) throw std::invalid_argument("max_steps must be in [1, 5e6]");
}

// Generic fixed-step driver. stage(f, t, y, h) performs one step.
template <typename Step>
OdeResult fixed_step(const char* name, int evals_per_step, const OdeFn& f, double t0, const Vec& y0, double t1, double h,
                     const OdeOptions& opt, Step step) {
    validate(y0, h, opt, true);
    OdeResult r;
    r.method = name;
    const double dir = (t1 >= t0) ? 1.0 : -1.0;
    const double span = abs(t1 - t0);
    const double nsteps_d = ceil(span / h - 1e-12);
    if (nsteps_d > opt.max_steps) {
        r.message = "too many steps: " + to_string(nsteps_d, 6) + " > max_steps";
        return r;
    }
    const int n = static_cast<int>(nsteps_d);
    r.t.reserve(static_cast<std::size_t>(n) + 1);
    r.y.reserve(static_cast<std::size_t>(n) + 1);
    double t = t0;
    Vec y = y0;
    r.t.push_back(t);
    r.y.push_back(y);
    for (int i = 0; i < n; ++i) {
        double hs = dir * h;
        if (dir * (t + hs - t1) > 0) hs = t1 - t; // last partial step
        y = step(f, t, y, hs);
        t = (i + 1 == n) ? t1 : t + hs;
        r.evaluations += evals_per_step;
        ++r.steps;
        if (!finite(y)) {
            r.message = "solution became non-finite at t = " + to_string(t, 10);
            r.t.push_back(t);
            r.y.push_back(y);
            return r;
        }
        r.t.push_back(t);
        r.y.push_back(y);
    }
    r.success = true;
    r.message = "completed";
    return r;
}

} // namespace

OdeResult euler(const OdeFn& f, double t0, const Vec& y0, double t1, double h, const OdeOptions& opt) {
    return fixed_step("euler", 1, f, t0, y0, t1, h, opt, [](const OdeFn& fn, double t, const Vec& y, double hs) {
        return axpy(y, hs, fn(t, y));
    });
}

OdeResult heun(const OdeFn& f, double t0, const Vec& y0, double t1, double h, const OdeOptions& opt) {
    return fixed_step("heun", 2, f, t0, y0, t1, h, opt, [](const OdeFn& fn, double t, const Vec& y, double hs) {
        const Vec k1 = fn(t, y);
        const Vec k2 = fn(t + hs, axpy(y, hs, k1));
        Vec r(y.size());
        for (std::size_t i = 0; i < y.size(); ++i) r[i] = y[i] + 0.5 * hs * (k1[i] + k2[i]);
        return r;
    });
}

OdeResult midpoint_method(const OdeFn& f, double t0, const Vec& y0, double t1, double h, const OdeOptions& opt) {
    return fixed_step("midpoint", 2, f, t0, y0, t1, h, opt, [](const OdeFn& fn, double t, const Vec& y, double hs) {
        const Vec k1 = fn(t, y);
        const Vec k2 = fn(t + 0.5 * hs, axpy(y, 0.5 * hs, k1));
        return axpy(y, hs, k2);
    });
}

OdeResult rk4(const OdeFn& f, double t0, const Vec& y0, double t1, double h, const OdeOptions& opt) {
    return fixed_step("rk4", 4, f, t0, y0, t1, h, opt, [](const OdeFn& fn, double t, const Vec& y, double hs) {
        const Vec k1 = fn(t, y);
        const Vec k2 = fn(t + 0.5 * hs, axpy(y, 0.5 * hs, k1));
        const Vec k3 = fn(t + 0.5 * hs, axpy(y, 0.5 * hs, k2));
        const Vec k4 = fn(t + hs, axpy(y, hs, k3));
        Vec r(y.size());
        for (std::size_t i = 0; i < y.size(); ++i) r[i] = y[i] + hs / 6.0 * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
        return r;
    });
}

// Dormand–Prince 5(4) with adaptive step size control.
OdeResult rk45(const OdeFn& f, double t0, const Vec& y0, double t1, double h0, const OdeOptions& opt) {
    validate(y0, 1.0, opt, false);
    if (!(opt.tol > 0.0)) throw std::invalid_argument("tol must be positive");
    OdeResult r;
    r.method = "rk45";
    const double dir = (t1 >= t0) ? 1.0 : -1.0;
    const double span = abs(t1 - t0);
    if (span == 0.0) { r.t = {t0}; r.y = {y0}; r.success = true; r.message = "completed"; return r; }
    double h = (h0 > 0.0) ? h0 : span / 100.0;
    if (opt.h_max > 0.0) h = fmin(h, opt.h_max);
    h = fmin(h, span);

    static const double c2 = 1.0 / 5, c3 = 3.0 / 10, c4 = 4.0 / 5, c5 = 8.0 / 9;
    static const double a21 = 1.0 / 5;
    static const double a31 = 3.0 / 40, a32 = 9.0 / 40;
    static const double a41 = 44.0 / 45, a42 = -56.0 / 15, a43 = 32.0 / 9;
    static const double a51 = 19372.0 / 6561, a52 = -25360.0 / 2187, a53 = 64448.0 / 6561, a54 = -212.0 / 729;
    static const double a61 = 9017.0 / 3168, a62 = -355.0 / 33, a63 = 46732.0 / 5247, a64 = 49.0 / 176, a65 = -5103.0 / 18656;
    static const double b1 = 35.0 / 384, b3 = 500.0 / 1113, b4 = 125.0 / 192, b5 = -2187.0 / 6784, b6 = 11.0 / 84;
    static const double e1 = 71.0 / 57600, e3 = -71.0 / 16695, e4 = 71.0 / 1920, e5 = -17253.0 / 339200, e6 = 22.0 / 525, e7 = -1.0 / 40;

    double t = t0;
    Vec y = y0;
    const std::size_t n = y.size();
    r.t.push_back(t);
    r.y.push_back(y);
    Vec k1 = f(t, y);
    r.evaluations = 1;
    Vec k2(n), k3(n), k4(n), k5(n), k6(n), k7(n), ytmp(n), ynew(n);
    int attempts = 0;
    while (dir * (t1 - t) > 1e-15 * fmax(1.0, abs(t1))) {
        if (++attempts > opt.max_steps * 4) { r.message = "maximum number of steps exceeded"; return r; }
        if (r.steps >= opt.max_steps) { r.message = "maximum number of steps exceeded"; return r; }
        if (dir * (t + dir * h - t1) > 0) h = abs(t1 - t);
        const double hs = dir * h;
        for (std::size_t i = 0; i < n; ++i) ytmp[i] = y[i] + hs * a21 * k1[i];
        k2 = f(t + c2 * hs, ytmp);
        for (std::size_t i = 0; i < n; ++i) ytmp[i] = y[i] + hs * (a31 * k1[i] + a32 * k2[i]);
        k3 = f(t + c3 * hs, ytmp);
        for (std::size_t i = 0; i < n; ++i) ytmp[i] = y[i] + hs * (a41 * k1[i] + a42 * k2[i] + a43 * k3[i]);
        k4 = f(t + c4 * hs, ytmp);
        for (std::size_t i = 0; i < n; ++i) ytmp[i] = y[i] + hs * (a51 * k1[i] + a52 * k2[i] + a53 * k3[i] + a54 * k4[i]);
        k5 = f(t + c5 * hs, ytmp);
        for (std::size_t i = 0; i < n; ++i) ytmp[i] = y[i] + hs * (a61 * k1[i] + a62 * k2[i] + a63 * k3[i] + a64 * k4[i] + a65 * k5[i]);
        k6 = f(t + hs, ytmp);
        for (std::size_t i = 0; i < n; ++i) ynew[i] = y[i] + hs * (b1 * k1[i] + b3 * k3[i] + b4 * k4[i] + b5 * k5[i] + b6 * k6[i]);
        k7 = f(t + hs, ynew);
        r.evaluations += 6;
        // error estimate
        double err = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double ei = hs * (e1 * k1[i] + e3 * k3[i] + e4 * k4[i] + e5 * k5[i] + e6 * k6[i] + e7 * k7[i]);
            const double sc = opt.tol * fmax(1.0, fmax(abs(y[i]), abs(ynew[i])));
            const double q = ei / sc;
            err += q * q;
        }
        err = sqrt(err / n);
        if (!is_finite(err)) {
            h *= 0.1;
            ++r.rejected;
            if (h < opt.h_min) { r.message = "step size underflow (solution may be non-finite)"; return r; }
            continue;
        }
        if (err <= 1.0) {
            t = (dir * (t + hs - t1) >= 0) ? t1 : t + hs;
            y = ynew;
            k1 = k7; // FSAL
            ++r.steps;
            r.t.push_back(t);
            r.y.push_back(y);
            double factor = (err == 0.0) ? 5.0 : 0.9 * pow(err, -0.2);
            factor = clamp(factor, 0.2, 5.0);
            h *= factor;
            if (opt.h_max > 0.0) h = fmin(h, opt.h_max);
        } else {
            ++r.rejected;
            double factor = 0.9 * pow(err, -0.25);
            factor = clamp(factor, 0.1, 0.9);
            h *= factor;
            if (h < opt.h_min) { r.message = "step size fell below h_min"; return r; }
        }
    }
    r.success = true;
    r.message = "completed";
    return r;
}

OdeResult solve_scalar(const std::string& method, const std::function<double(double, double)>& f, double t0, double y0,
                       double t1, double h, const OdeOptions& opt) {
    OdeFn vf = [&f](double t, const Vec& y) { return Vec{f(t, y[0])}; };
    const Vec y0v{y0};
    if (method == "euler") return euler(vf, t0, y0v, t1, h, opt);
    if (method == "heun") return heun(vf, t0, y0v, t1, h, opt);
    if (method == "midpoint") return midpoint_method(vf, t0, y0v, t1, h, opt);
    if (method == "rk4") return rk4(vf, t0, y0v, t1, h, opt);
    if (method == "rk45") return rk45(vf, t0, y0v, t1, h, opt);
    throw std::invalid_argument("unknown ODE method '" + method + "'");
}

} // namespace cml
