// cml/integrate.cpp — numerical quadrature.
#include "cml/integrate.hpp"
#include "cml/core.hpp"

#include <map>
#include <stdexcept>

namespace cml {

namespace {
void check_interval(double a, double b) {
    if (is_nan(a) || is_nan(b)) throw std::invalid_argument("integration bounds must be numbers");
}
void check_n(int n, int max_n = 10000000) {
    if (n < 1) throw std::invalid_argument("n must be >= 1");
    if (n > max_n) throw std::invalid_argument("n is too large (limit " + std::to_string(max_n) + ")");
}
} // namespace

IntegralResult midpoint(const RealFn& f, double a, double b, int n) {
    check_interval(a, b);
    check_n(n);
    IntegralResult r;
    r.method = "midpoint";
    const double h = (b - a) / n;
    double s = 0.0;
    for (int i = 0; i < n; ++i) s += f(a + (i + 0.5) * h);
    r.value = s * h;
    r.evaluations = n;
    r.subdivisions = n;
    r.converged = is_finite(r.value);
    return r;
}

IntegralResult trapezoid(const RealFn& f, double a, double b, int n) {
    check_interval(a, b);
    check_n(n);
    IntegralResult r;
    r.method = "trapezoid";
    const double h = (b - a) / n;
    double s = 0.5 * (f(a) + f(b));
    for (int i = 1; i < n; ++i) s += f(a + i * h);
    r.value = s * h;
    r.evaluations = n + 1;
    r.subdivisions = n;
    r.converged = is_finite(r.value);
    return r;
}

IntegralResult simpson(const RealFn& f, double a, double b, int n) {
    check_interval(a, b);
    check_n(n);
    if (n % 2) ++n;
    IntegralResult r;
    r.method = "simpson";
    const double h = (b - a) / n;
    double s = f(a) + f(b);
    for (int i = 1; i < n; ++i) s += (i % 2 ? 4.0 : 2.0) * f(a + i * h);
    r.value = s * h / 3.0;
    r.evaluations = n + 1;
    r.subdivisions = n;
    r.converged = is_finite(r.value);
    return r;
}

IntegralResult simpson38(const RealFn& f, double a, double b, int n) {
    check_interval(a, b);
    check_n(n);
    while (n % 3) ++n;
    IntegralResult r;
    r.method = "simpson38";
    const double h = (b - a) / n;
    double s = f(a) + f(b);
    for (int i = 1; i < n; ++i) s += (i % 3 ? 3.0 : 2.0) * f(a + i * h);
    r.value = s * 3.0 * h / 8.0;
    r.evaluations = n + 1;
    r.subdivisions = n;
    r.converged = is_finite(r.value);
    return r;
}

IntegralResult boole(const RealFn& f, double a, double b, int n) {
    check_interval(a, b);
    check_n(n);
    while (n % 4) ++n;
    IntegralResult r;
    r.method = "boole";
    const double h = (b - a) / n;
    double s = 7.0 * (f(a) + f(b));
    for (int i = 1; i < n; ++i) {
        const int m = i % 4;
        const double w = (m == 0) ? 14.0 : (m == 2 ? 12.0 : 32.0);
        s += w * f(a + i * h);
    }
    r.value = s * 2.0 * h / 45.0;
    r.evaluations = n + 1;
    r.subdivisions = n;
    r.converged = is_finite(r.value);
    return r;
}

IntegralResult romberg(const RealFn& f, double a, double b, int max_levels, double tol) {
    check_interval(a, b);
    if (max_levels < 1 || max_levels > 30) throw std::invalid_argument("max_levels must be in [1, 30]");
    if (!(tol > 0.0)) throw std::invalid_argument("tol must be positive");
    IntegralResult r;
    r.method = "romberg";
    std::vector<std::vector<double>> R;
    double h = b - a;
    R.push_back({0.5 * h * (f(a) + f(b))});
    r.evaluations = 2;
    for (int k = 1; k < max_levels; ++k) {
        h *= 0.5;
        double s = 0.0;
        const int terms = 1 << (k - 1);
        for (int i = 1; i <= terms; ++i) s += f(a + (2 * i - 1) * h);
        r.evaluations += terms;
        std::vector<double> row(static_cast<std::size_t>(k) + 1);
        row[0] = 0.5 * R[k - 1][0] + h * s;
        double p4 = 4.0;
        for (int j = 1; j <= k; ++j) {
            row[j] = row[j - 1] + (row[j - 1] - R[k - 1][j - 1]) / (p4 - 1.0);
            p4 *= 4.0;
        }
        R.push_back(row);
        const double err = abs(row[k] - R[k - 1][k - 1]);
        r.value = row[k];
        r.error_estimate = err;
        r.subdivisions = k + 1;
        if (k >= 3 && err <= tol * fmax(1.0, abs(row[k]))) {
            r.table = R;
            r.converged = true;
            r.message = "converged";
            return r;
        }
    }
    r.table = R;
    r.converged = false;
    r.message = "did not reach tolerance within max_levels";
    return r;
}

// Gauss–Legendre nodes via Newton iteration on P_n(x).
void gauss_legendre_nodes(int n, std::vector<double>& nodes, std::vector<double>& weights) {
    if (n < 1 || n > 64) throw std::invalid_argument("gauss_legendre points must be in [1, 64]");
    static std::map<int, std::pair<std::vector<double>, std::vector<double>>> cache;
    auto it = cache.find(n);
    if (it != cache.end()) { nodes = it->second.first; weights = it->second.second; return; }
    nodes.assign(static_cast<std::size_t>(n), 0.0);
    weights.assign(static_cast<std::size_t>(n), 0.0);
    const int m = (n + 1) / 2;
    for (int i = 0; i < m; ++i) {
        double x = cos(constants::pi * (i + 0.75) / (n + 0.5));
        double dp = 0.0;
        for (int iter = 0; iter < 100; ++iter) {
            double p0 = 1.0, p1 = x;
            for (int k = 2; k <= n; ++k) {
                const double p2 = ((2.0 * k - 1.0) * x * p1 - (k - 1.0) * p0) / k;
                p0 = p1;
                p1 = p2;
            }
            const double pn = (n == 1) ? x : p1;
            const double pn1 = (n == 1) ? 1.0 : p0;
            dp = n * (x * pn - pn1) / (x * x - 1.0);
            const double dx = pn / dp;
            x -= dx;
            if (abs(dx) < 1e-16) break;
        }
        nodes[static_cast<std::size_t>(i)] = -x;
        nodes[static_cast<std::size_t>(n - 1 - i)] = x;
        const double w = 2.0 / ((1.0 - x * x) * dp * dp);
        weights[static_cast<std::size_t>(i)] = w;
        weights[static_cast<std::size_t>(n - 1 - i)] = w;
    }
    cache[n] = {nodes, weights};
}

IntegralResult gauss_legendre(const RealFn& f, double a, double b, int points) {
    check_interval(a, b);
    std::vector<double> x, w;
    gauss_legendre_nodes(points, x, w);
    IntegralResult r;
    r.method = "gauss_legendre";
    const double c = 0.5 * (b - a), d = 0.5 * (b + a);
    double s = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) s += w[i] * f(c * x[i] + d);
    r.value = c * s;
    r.evaluations = points;
    r.subdivisions = points;
    r.converged = is_finite(r.value);
    return r;
}

namespace {
struct Adaptive {
    const RealFn& f;
    int evals = 0;
    int max_depth;
    int deepest = 0;
    bool hit_limit = false;
    Adaptive(const RealFn& fn, int md) : f(fn), max_depth(md) {}
    double rec(double a, double b, double fa, double fm, double fb, double whole, double tol, int depth) {
        const double m = 0.5 * (a + b);
        const double lm = 0.5 * (a + m), rm = 0.5 * (m + b);
        const double flm = f(lm), frm = f(rm);
        evals += 2;
        const double left = (m - a) / 6.0 * (fa + 4.0 * flm + fm);
        const double right = (b - m) / 6.0 * (fm + 4.0 * frm + fb);
        const double delta = left + right - whole;
        if (depth > deepest) deepest = depth;
        if (depth >= max_depth || evals > 2000000) { hit_limit = true; return left + right + delta / 15.0; }
        if (abs(delta) <= 15.0 * tol || !is_finite(delta)) return left + right + delta / 15.0;
        return rec(a, m, fa, flm, fm, left, tol * 0.5, depth + 1) + rec(m, b, fm, frm, fb, right, tol * 0.5, depth + 1);
    }
};
} // namespace

IntegralResult adaptive_simpson(const RealFn& f, double a, double b, double tol, int max_depth) {
    check_interval(a, b);
    if (!(tol > 0.0)) throw std::invalid_argument("tol must be positive");
    if (max_depth < 1 || max_depth > 60) throw std::invalid_argument("max_depth must be in [1, 60]");
    IntegralResult r;
    r.method = "adaptive_simpson";
    Adaptive ad(f, max_depth);
    const double fa = f(a), fb = f(b), fm = f(0.5 * (a + b));
    ad.evals = 3;
    const double whole = (b - a) / 6.0 * (fa + 4.0 * fm + fb);
    r.value = ad.rec(a, b, fa, fm, fb, whole, tol, 0);
    r.evaluations = ad.evals;
    r.subdivisions = ad.deepest;
    r.error_estimate = tol;
    r.converged = !ad.hit_limit && is_finite(r.value);
    r.message = ad.hit_limit ? "maximum recursion depth reached in some subintervals" : "converged";
    return r;
}

IntegralResult monte_carlo(const RealFn& f, double a, double b, int samples, unsigned long long seed) {
    check_interval(a, b);
    check_n(samples, 50000000);
    IntegralResult r;
    r.method = "monte_carlo";
    // xorshift64* generator: deterministic and portable.
    unsigned long long s = seed ? seed : 0x9E3779B97F4A7C15ULL;
    double sum = 0.0, sum2 = 0.0;
    for (int i = 0; i < samples; ++i) {
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        const double u = static_cast<double>((s * 2685821657736338717ULL) >> 11) * (1.0 / 9007199254740992.0);
        const double v = f(a + (b - a) * u);
        sum += v;
        sum2 += v * v;
    }
    const double mean = sum / samples;
    const double var = fmax(0.0, sum2 / samples - mean * mean);
    r.value = (b - a) * mean;
    r.error_estimate = abs(b - a) * sqrt(var / samples);
    r.evaluations = samples;
    r.subdivisions = samples;
    r.converged = is_finite(r.value);
    return r;
}

IntegralResult integrate(const RealFn& f, double a, double b, double tol) {
    check_interval(a, b);
    if (a == b) { IntegralResult r; r.method = "integrate"; return r; }
    double sgn = 1.0;
    if (a > b) { const double t = a; a = b; b = t; sgn = -1.0; }
    IntegralResult r;
    if (is_inf(a) && is_inf(b)) {
        // x = t / (1 - t^2), t in (-1, 1)
        RealFn g = [&f](double t) {
            const double d = 1.0 - t * t;
            if (d <= 0.0) return 0.0;
            const double x = t / d;
            const double v = f(x) * (1.0 + t * t) / (d * d);
            return is_finite(v) ? v : 0.0;
        };
        r = adaptive_simpson(g, -1.0 + 1e-12, 1.0 - 1e-12, tol, 50);
    } else if (is_inf(b)) {
        // x = a + t / (1 - t), t in [0, 1)
        RealFn g = [&f, a](double t) {
            const double d = 1.0 - t;
            if (d <= 0.0) return 0.0;
            const double v = f(a + t / d) / (d * d);
            return is_finite(v) ? v : 0.0;
        };
        r = adaptive_simpson(g, 0.0, 1.0 - 1e-12, tol, 50);
    } else if (is_inf(a)) {
        RealFn g = [&f, b](double t) {
            const double d = 1.0 - t;
            if (d <= 0.0) return 0.0;
            const double v = f(b - t / d) / (d * d);
            return is_finite(v) ? v : 0.0;
        };
        r = adaptive_simpson(g, 0.0, 1.0 - 1e-12, tol, 50);
    } else {
        r = adaptive_simpson(f, a, b, tol, 50);
    }
    r.method = "integrate";
    r.value *= sgn;
    return r;
}

} // namespace cml
