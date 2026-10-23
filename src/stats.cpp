// cml/stats.cpp — statistics.
#include "cml/stats.hpp"
#include "cml/core.hpp"

#include <algorithm>
#include <map>
#include <numeric>
#include <stdexcept>

namespace cml {

namespace {
void check(const std::vector<double>& d, std::size_t min_n = 1) {
    if (d.size() < min_n) throw std::invalid_argument("need at least " + std::to_string(min_n) + " data points");
    if (d.size() > 10000000) throw std::invalid_argument("too many data points (limit 1e7)");
    for (double v : d)
        if (!is_finite(v)) throw std::invalid_argument("data must be finite");
}
void check_pair(const std::vector<double>& x, const std::vector<double>& y, std::size_t min_n) {
    check(x, min_n);
    check(y, min_n);
    if (x.size() != y.size()) throw std::invalid_argument("x and y must have the same length");
}
std::vector<double> sorted(const std::vector<double>& d) {
    std::vector<double> s = d;
    std::sort(s.begin(), s.end());
    return s;
}
double quantile_sorted(const std::vector<double>& s, double p) {
    if (s.size() == 1) return s[0];
    const double h = (s.size() - 1) * p;
    const std::size_t lo = static_cast<std::size_t>(floor(h));
    const std::size_t hi = std::min(lo + 1, s.size() - 1);
    return s[lo] + (h - lo) * (s[hi] - s[lo]);
}
} // namespace

double mean(const std::vector<double>& d) {
    check(d);
    // Kahan-compensated sum for long inputs.
    double sum = 0.0, c = 0.0;
    for (double v : d) {
        const double y = v - c;
        const double t = sum + y;
        c = (t - sum) - y;
        sum = t;
    }
    return sum / d.size();
}

double median(const std::vector<double>& d) {
    check(d);
    return quantile_sorted(sorted(d), 0.5);
}

double mode(const std::vector<double>& d) {
    check(d);
    std::map<double, std::size_t> counts;
    for (double v : d) ++counts[v];
    std::size_t best = 0;
    double value = nan();
    for (const auto& kv : counts)
        if (kv.second > best) { best = kv.second; value = kv.first; }
    return best > 1 ? value : nan();
}

double variance(const std::vector<double>& d, bool sample) {
    check(d, sample ? 2 : 1);
    const double m = mean(d);
    double s = 0.0;
    for (double v : d) s += (v - m) * (v - m);
    return s / (sample ? d.size() - 1 : d.size());
}
double stddev(const std::vector<double>& d, bool sample) { return sqrt(variance(d, sample)); }

double quantile(const std::vector<double>& d, double p) {
    check(d);
    if (!(p >= 0.0 && p <= 1.0)) throw std::invalid_argument("p must be in [0, 1]");
    return quantile_sorted(sorted(d), p);
}

double covariance(const std::vector<double>& x, const std::vector<double>& y, bool sample) {
    check_pair(x, y, sample ? 2 : 1);
    const double mx = mean(x), my = mean(y);
    double s = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) s += (x[i] - mx) * (y[i] - my);
    return s / (sample ? x.size() - 1 : x.size());
}

double correlation(const std::vector<double>& x, const std::vector<double>& y) {
    check_pair(x, y, 2);
    const double mx = mean(x), my = mean(y);
    double sxy = 0.0, sxx = 0.0, syy = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        sxy += (x[i] - mx) * (y[i] - my);
        sxx += (x[i] - mx) * (x[i] - mx);
        syy += (y[i] - my) * (y[i] - my);
    }
    if (sxx == 0.0 || syy == 0.0) return nan();
    return sxy / sqrt(sxx * syy);
}

std::vector<double> ranks(const std::vector<double>& d) {
    check(d);
    std::vector<std::size_t> idx(d.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) { return d[a] < d[b]; });
    std::vector<double> r(d.size());
    std::size_t i = 0;
    while (i < idx.size()) {
        std::size_t j = i;
        while (j + 1 < idx.size() && d[idx[j + 1]] == d[idx[i]]) ++j;
        const double avg = (i + j) / 2.0 + 1.0;
        for (std::size_t k = i; k <= j; ++k) r[idx[k]] = avg;
        i = j + 1;
    }
    return r;
}

double spearman(const std::vector<double>& x, const std::vector<double>& y) {
    check_pair(x, y, 2);
    return correlation(ranks(x), ranks(y));
}

std::vector<double> z_scores(const std::vector<double>& d) {
    check(d, 2);
    const double m = mean(d), s = stddev(d, true);
    std::vector<double> z(d.size());
    for (std::size_t i = 0; i < d.size(); ++i) z[i] = s == 0.0 ? 0.0 : (d[i] - m) / s;
    return z;
}

std::vector<double> moving_average(const std::vector<double>& d, std::size_t window) {
    check(d);
    if (window < 1 || window > d.size()) throw std::invalid_argument("window must be in [1, n]");
    std::vector<double> out(d.size() - window + 1);
    double s = 0.0;
    for (std::size_t i = 0; i < window; ++i) s += d[i];
    out[0] = s / window;
    for (std::size_t i = window; i < d.size(); ++i) {
        s += d[i] - d[i - window];
        out[i - window + 1] = s / window;
    }
    return out;
}

std::vector<double> cumulative_sum(const std::vector<double>& d) {
    check(d);
    std::vector<double> out(d.size());
    double s = 0.0;
    for (std::size_t i = 0; i < d.size(); ++i) { s += d[i]; out[i] = s; }
    return out;
}

std::vector<double> min_max_normalize(const std::vector<double>& d) {
    check(d);
    const double lo = *std::min_element(d.begin(), d.end());
    const double hi = *std::max_element(d.begin(), d.end());
    std::vector<double> out(d.size());
    for (std::size_t i = 0; i < d.size(); ++i) out[i] = (hi == lo) ? 0.0 : (d[i] - lo) / (hi - lo);
    return out;
}

Descriptive describe(const std::vector<double>& d) {
    check(d);
    Descriptive s;
    const std::vector<double> v = sorted(d);
    const std::size_t n = v.size();
    s.count = n;
    s.mean = mean(d);
    s.sum = s.mean * n;
    s.median = quantile_sorted(v, 0.5);
    s.mode = mode(d);
    s.min = v.front();
    s.max = v.back();
    s.range = s.max - s.min;
    s.q1 = quantile_sorted(v, 0.25);
    s.q3 = quantile_sorted(v, 0.75);
    s.iqr = s.q3 - s.q1;
    double m2 = 0.0, m3 = 0.0, m4 = 0.0, mad = 0.0;
    for (double x : v) {
        const double dx = x - s.mean;
        m2 += dx * dx;
        m3 += dx * dx * dx;
        m4 += dx * dx * dx * dx;
        mad += abs(dx);
    }
    s.variance_population = m2 / n;
    s.stddev_population = sqrt(s.variance_population);
    s.variance = n > 1 ? m2 / (n - 1) : 0.0;
    s.stddev = sqrt(s.variance);
    s.std_error = n > 1 ? s.stddev / sqrt(static_cast<double>(n)) : 0.0;
    s.mean_abs_dev = mad / n;
    s.coefficient_of_variation = s.mean != 0.0 ? s.stddev / s.mean : nan();
    if (n > 2 && m2 > 0.0) {
        const double g1 = (m3 / n) / pow(m2 / n, 1.5);
        s.skewness = sqrt(static_cast<double>(n) * (n - 1)) / (n - 2) * g1;
    } else {
        s.skewness = nan();
    }
    if (n > 3 && m2 > 0.0) {
        const double g2 = (m4 / n) / ((m2 / n) * (m2 / n)) - 3.0;
        s.kurtosis = (static_cast<double>(n) - 1) / ((n - 2.0) * (n - 3.0)) * ((n + 1.0) * g2 + 6.0);
    } else {
        s.kurtosis = nan();
    }
    bool positive = true;
    double logsum = 0.0, invsum = 0.0;
    for (double x : v) {
        if (x <= 0.0) { positive = false; break; }
        logsum += ln(x);
        invsum += 1.0 / x;
    }
    s.geometric_mean = positive ? exp(logsum / n) : nan();
    s.harmonic_mean = positive ? n / invsum : nan();
    return s;
}

LinearRegression linear_regression(const std::vector<double>& x, const std::vector<double>& y) {
    check_pair(x, y, 2);
    const std::size_t n = x.size();
    const double mx = mean(x), my = mean(y);
    double sxy = 0.0, sxx = 0.0, syy = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        sxy += (x[i] - mx) * (y[i] - my);
        sxx += (x[i] - mx) * (x[i] - mx);
        syy += (y[i] - my) * (y[i] - my);
    }
    if (sxx == 0.0) throw std::invalid_argument("x values are all identical; slope is undefined");
    LinearRegression r;
    r.slope = sxy / sxx;
    r.intercept = my - r.slope * mx;
    r.r = (syy == 0.0) ? 1.0 : sxy / sqrt(sxx * syy);
    r.r_squared = r.r * r.r;
    r.residuals.resize(n);
    double ss_res = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        r.residuals[i] = y[i] - (r.intercept + r.slope * x[i]);
        ss_res += r.residuals[i] * r.residuals[i];
    }
    r.rmse = sqrt(ss_res / n);
    if (n > 2) {
        const double s2 = ss_res / (n - 2);
        r.std_error_slope = sqrt(s2 / sxx);
        r.std_error_intercept = sqrt(s2 * (1.0 / n + mx * mx / sxx));
    }
    return r;
}

Histogram histogram(const std::vector<double>& d, std::size_t bins) {
    check(d);
    if (bins == 0) bins = static_cast<std::size_t>(ceil(log2(static_cast<double>(d.size())) + 1.0));
    if (bins > 10000) throw std::invalid_argument("bins limit is 10000");
    Histogram h;
    const double lo = *std::min_element(d.begin(), d.end());
    double hi = *std::max_element(d.begin(), d.end());
    if (hi == lo) hi = lo + 1.0;
    const double width = (hi - lo) / bins;
    h.edges.resize(bins + 1);
    for (std::size_t i = 0; i <= bins; ++i) h.edges[i] = lo + i * width;
    h.counts.assign(bins, 0);
    for (double v : d) {
        std::size_t b = static_cast<std::size_t>((v - lo) / width);
        if (b >= bins) b = bins - 1;
        ++h.counts[b];
    }
    h.density.resize(bins);
    for (std::size_t i = 0; i < bins; ++i) h.density[i] = h.counts[i] / (d.size() * width);
    return h;
}

} // namespace cml
