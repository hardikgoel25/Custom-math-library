// cml/stats.hpp — descriptive statistics, correlation, regression, histograms.
#ifndef CML_STATS_HPP
#define CML_STATS_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace cml {

struct Descriptive {
    std::size_t count = 0;
    double sum = 0.0;
    double mean = 0.0;
    double median = 0.0;
    double mode = 0.0;            // NaN when every value is unique
    double min = 0.0;
    double max = 0.0;
    double range = 0.0;
    double variance = 0.0;        // sample (n-1)
    double variance_population = 0.0;
    double stddev = 0.0;
    double stddev_population = 0.0;
    double std_error = 0.0;
    double q1 = 0.0, q3 = 0.0, iqr = 0.0;
    double skewness = 0.0;        // sample skewness (adjusted Fisher–Pearson)
    double kurtosis = 0.0;        // excess kurtosis
    double geometric_mean = 0.0;  // NaN if any value <= 0
    double harmonic_mean = 0.0;   // NaN if any value <= 0
    double mean_abs_dev = 0.0;
    double coefficient_of_variation = 0.0;
};

Descriptive describe(const std::vector<double>& data);
double mean(const std::vector<double>& data);
double median(const std::vector<double>& data);
double mode(const std::vector<double>& data);
double variance(const std::vector<double>& data, bool sample = true);
double stddev(const std::vector<double>& data, bool sample = true);
double quantile(const std::vector<double>& data, double p);   // linear interpolation (R type 7)
double covariance(const std::vector<double>& x, const std::vector<double>& y, bool sample = true);
double correlation(const std::vector<double>& x, const std::vector<double>& y); // Pearson
double spearman(const std::vector<double>& x, const std::vector<double>& y);
std::vector<double> ranks(const std::vector<double>& data);               // average ranks for ties
std::vector<double> z_scores(const std::vector<double>& data);
std::vector<double> moving_average(const std::vector<double>& data, std::size_t window);
std::vector<double> cumulative_sum(const std::vector<double>& data);
std::vector<double> min_max_normalize(const std::vector<double>& data);

struct LinearRegression {
    double slope = 0.0;
    double intercept = 0.0;
    double r = 0.0;
    double r_squared = 0.0;
    double std_error_slope = 0.0;
    double std_error_intercept = 0.0;
    double rmse = 0.0;
    std::vector<double> residuals;
};
LinearRegression linear_regression(const std::vector<double>& x, const std::vector<double>& y);

struct Histogram {
    std::vector<double> edges;         // bins + 1 edges
    std::vector<std::size_t> counts;   // per bin
    std::vector<double> density;       // counts / (n * width)
};
Histogram histogram(const std::vector<double>& data, std::size_t bins = 0); // 0 = Sturges' rule

} // namespace cml

#endif // CML_STATS_HPP
