// cml/interpolate.cpp — interpolation and fitting.
#include "cml/interpolate.hpp"
#include "cml/core.hpp"
#include "cml/matrix.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace cml {

namespace {
void check_points(const std::vector<double>& xs, const std::vector<double>& ys, std::size_t min_points) {
    if (xs.size() != ys.size()) throw std::invalid_argument("xs and ys must have the same length");
    if (xs.size() < min_points) throw std::invalid_argument("need at least " + std::to_string(min_points) + " points");
    if (xs.size() > 100000) throw std::invalid_argument("too many points (limit 100000)");
    for (std::size_t i = 0; i < xs.size(); ++i)
        if (!is_finite(xs[i]) || !is_finite(ys[i])) throw std::invalid_argument("points must be finite");
}
void check_distinct(const std::vector<double>& xs) {
    for (std::size_t i = 0; i < xs.size(); ++i)
        for (std::size_t j = i + 1; j < xs.size() && j < i + 2000; ++j)
            if (xs[i] == xs[j]) throw std::invalid_argument("x values must be distinct");
}
} // namespace

void sort_points(std::vector<double>& xs, std::vector<double>& ys) {
    check_points(xs, ys, 1);
    std::vector<std::size_t> idx(xs.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) { return xs[a] < xs[b]; });
    std::vector<double> x2(xs.size()), y2(ys.size());
    for (std::size_t i = 0; i < idx.size(); ++i) { x2[i] = xs[idx[i]]; y2[i] = ys[idx[i]]; }
    for (std::size_t i = 1; i < x2.size(); ++i)
        if (x2[i] == x2[i - 1]) throw std::invalid_argument("x values must be distinct");
    xs = x2;
    ys = y2;
}

double lagrange_interpolate(const std::vector<double>& xs, const std::vector<double>& ys, double x) {
    check_points(xs, ys, 1);
    const std::size_t n = xs.size();
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        double term = ys[i];
        for (std::size_t j = 0; j < n; ++j) {
            if (i == j) continue;
            const double d = xs[i] - xs[j];
            if (d == 0.0) throw std::invalid_argument("x values must be distinct");
            term *= (x - xs[j]) / d;
        }
        sum += term;
    }
    return sum;
}

Polynomial lagrange_polynomial(const std::vector<double>& xs, const std::vector<double>& ys) {
    check_points(xs, ys, 1);
    if (xs.size() > 200) throw std::invalid_argument("lagrange_polynomial supports up to 200 points");
    check_distinct(xs);
    const std::size_t n = xs.size();
    Polynomial result;
    for (std::size_t i = 0; i < n; ++i) {
        Polynomial basis({1.0});
        double denom = 1.0;
        for (std::size_t j = 0; j < n; ++j) {
            if (i == j) continue;
            basis *= Polynomial({-xs[j], 1.0});
            denom *= (xs[i] - xs[j]);
        }
        result += basis * (ys[i] / denom);
    }
    return result.cleaned();
}

double NewtonInterpolation::evaluate(double x) const {
    double r = 0.0;
    for (std::size_t i = coefficients.size(); i-- > 0;) r = r * (x - xs[i]) + coefficients[i];
    return r;
}

NewtonInterpolation newton_divided_differences(const std::vector<double>& xs, const std::vector<double>& ys) {
    check_points(xs, ys, 1);
    if (xs.size() > 200) throw std::invalid_argument("newton_divided_differences supports up to 200 points");
    check_distinct(xs);
    const std::size_t n = xs.size();
    NewtonInterpolation out;
    out.xs = xs;
    out.table.push_back(ys);
    for (std::size_t k = 1; k < n; ++k) {
        const std::vector<double>& prev = out.table[k - 1];
        std::vector<double> row(n - k);
        for (std::size_t i = 0; i < n - k; ++i) row[i] = (prev[i + 1] - prev[i]) / (xs[i + k] - xs[i]);
        out.table.push_back(row);
    }
    out.coefficients.resize(n);
    for (std::size_t k = 0; k < n; ++k) out.coefficients[k] = out.table[k][0];
    // expand to monomial basis
    Polynomial p;
    Polynomial basis({1.0});
    for (std::size_t k = 0; k < n; ++k) {
        p += basis * out.coefficients[k];
        basis *= Polynomial({-xs[k], 1.0});
    }
    out.polynomial = p.cleaned();
    return out;
}

double linear_interpolate(const std::vector<double>& xs, const std::vector<double>& ys, double x) {
    std::vector<double> sx = xs, sy = ys;
    sort_points(sx, sy);
    if (sx.size() == 1) return sy[0];
    if (x <= sx.front()) {
        const double t = (x - sx[0]) / (sx[1] - sx[0]);
        return sy[0] + t * (sy[1] - sy[0]);
    }
    if (x >= sx.back()) {
        const std::size_t n = sx.size();
        const double t = (x - sx[n - 2]) / (sx[n - 1] - sx[n - 2]);
        return sy[n - 2] + t * (sy[n - 1] - sy[n - 2]);
    }
    const auto it = std::upper_bound(sx.begin(), sx.end(), x);
    const std::size_t i = static_cast<std::size_t>(it - sx.begin()) - 1;
    const double t = (x - sx[i]) / (sx[i + 1] - sx[i]);
    return sy[i] + t * (sy[i + 1] - sy[i]);
}

// Cubic spline -----------------------------------------------------------------
CubicSpline CubicSpline::build(const std::vector<double>& xin, const std::vector<double>& yin, bool clamped, double d0, double dn) {
    std::vector<double> xs = xin, ys = yin;
    check_points(xs, ys, 2);
    sort_points(xs, ys);
    const std::size_t n = xs.size() - 1; // number of segments
    std::vector<double> h(n);
    for (std::size_t i = 0; i < n; ++i) h[i] = xs[i + 1] - xs[i];
    // Solve tridiagonal system for second-derivative-like coefficients c.
    std::vector<double> A(n + 1, 0.0), B(n + 1, 0.0), C(n + 1, 0.0), D(n + 1, 0.0);
    if (clamped) {
        B[0] = 2.0 * h[0]; C[0] = h[0]; D[0] = 3.0 * ((ys[1] - ys[0]) / h[0] - d0);
        A[n] = h[n - 1]; B[n] = 2.0 * h[n - 1]; D[n] = 3.0 * (dn - (ys[n] - ys[n - 1]) / h[n - 1]);
    } else {
        B[0] = 1.0; B[n] = 1.0;
    }
    for (std::size_t i = 1; i < n; ++i) {
        A[i] = h[i - 1];
        B[i] = 2.0 * (h[i - 1] + h[i]);
        C[i] = h[i];
        D[i] = 3.0 * ((ys[i + 1] - ys[i]) / h[i] - (ys[i] - ys[i - 1]) / h[i - 1]);
    }
    // Thomas algorithm
    std::vector<double> c(n + 1, 0.0);
    for (std::size_t i = 1; i <= n; ++i) {
        const double w = A[i] / B[i - 1];
        B[i] -= w * C[i - 1];
        D[i] -= w * D[i - 1];
    }
    c[n] = D[n] / B[n];
    for (std::size_t i = n; i-- > 0;) c[i] = (D[i] - C[i] * c[i + 1]) / B[i];
    CubicSpline s;
    s.segments_.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        Segment& g = s.segments_[i];
        g.x0 = xs[i];
        g.x1 = xs[i + 1];
        g.a = ys[i];
        g.c = c[i];
        g.b = (ys[i + 1] - ys[i]) / h[i] - h[i] * (2.0 * c[i] + c[i + 1]) / 3.0;
        g.d = (c[i + 1] - c[i]) / (3.0 * h[i]);
    }
    return s;
}
CubicSpline CubicSpline::natural(const std::vector<double>& xs, const std::vector<double>& ys) { return build(xs, ys, false, 0.0, 0.0); }
CubicSpline CubicSpline::clamped(const std::vector<double>& xs, const std::vector<double>& ys, double d0, double dn) { return build(xs, ys, true, d0, dn); }

const CubicSpline::Segment& CubicSpline::locate(double x) const {
    if (segments_.empty()) throw std::logic_error("empty spline");
    std::size_t lo = 0, hi = segments_.size() - 1;
    if (x <= segments_[0].x0) return segments_[0];
    if (x >= segments_[hi].x1) return segments_[hi];
    while (lo < hi) {
        const std::size_t mid = (lo + hi) / 2;
        if (x < segments_[mid].x0) hi = mid - 1;
        else if (x >= segments_[mid].x1) lo = mid + 1;
        else return segments_[mid];
    }
    return segments_[lo];
}
double CubicSpline::operator()(double x) const {
    const Segment& g = locate(x);
    const double t = x - g.x0;
    return g.a + t * (g.b + t * (g.c + t * g.d));
}
double CubicSpline::derivative(double x) const {
    const Segment& g = locate(x);
    const double t = x - g.x0;
    return g.b + t * (2.0 * g.c + 3.0 * t * g.d);
}
double CubicSpline::second_derivative(double x) const {
    const Segment& g = locate(x);
    const double t = x - g.x0;
    return 2.0 * g.c + 6.0 * t * g.d;
}
double CubicSpline::integral(double a, double b) const {
    double sgn = 1.0;
    if (a > b) { std::swap(a, b); sgn = -1.0; }
    double total = 0.0;
    for (const Segment& g : segments_) {
        const double lo = fmax(a, g.x0), hi = fmin(b, g.x1);
        if (hi <= lo) continue;
        auto F = [&](double x) {
            const double t = x - g.x0;
            return t * (g.a + t * (g.b / 2.0 + t * (g.c / 3.0 + t * g.d / 4.0)));
        };
        total += F(hi) - F(lo);
    }
    return sgn * total;
}

// Least squares ----------------------------------------------------------------
FitResult polyfit(const std::vector<double>& xs, const std::vector<double>& ys, int degree) {
    check_points(xs, ys, 1);
    if (degree < 0 || degree > 50) throw std::invalid_argument("degree must be in [0, 50]");
    if (static_cast<int>(xs.size()) <= degree) throw std::invalid_argument("need more points than the polynomial degree");
    const std::size_t n = xs.size();
    const std::size_t m = static_cast<std::size_t>(degree) + 1;
    // Scale x for conditioning: u = (x - mean) / scale
    double mean = 0.0;
    for (double x : xs) mean += x;
    mean /= n;
    double scale = 0.0;
    for (double x : xs) scale = fmax(scale, abs(x - mean));
    if (scale == 0.0) scale = 1.0;
    Matrix V(n, m, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        const double u = (xs[i] - mean) / scale;
        double p = 1.0;
        for (std::size_t j = 0; j < m; ++j) { V(i, j) = p; p *= u; }
    }
    const std::vector<double> coeff_u = V.solve_least_squares(ys);
    // Convert back to polynomial in x: p(u) with u = (x - mean)/scale
    Polynomial pu(coeff_u);
    Polynomial inner({-mean / scale, 1.0 / scale});
    FitResult out;
    out.polynomial = pu.compose(inner).cleaned();
    double ss_res = 0.0, ss_tot = 0.0, ymean = 0.0;
    for (double y : ys) ymean += y;
    ymean /= n;
    out.residuals.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double pred = pu.evaluate((xs[i] - mean) / scale);
        out.residuals[i] = ys[i] - pred;
        ss_res += out.residuals[i] * out.residuals[i];
        ss_tot += (ys[i] - ymean) * (ys[i] - ymean);
    }
    out.r_squared = (ss_tot == 0.0) ? 1.0 : 1.0 - ss_res / ss_tot;
    out.rmse = sqrt(ss_res / n);
    return out;
}

} // namespace cml
