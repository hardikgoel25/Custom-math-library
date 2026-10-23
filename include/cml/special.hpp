// cml/special.hpp — special functions and number theory helpers.
#ifndef CML_SPECIAL_HPP
#define CML_SPECIAL_HPP

#include <cstdint>
#include <vector>

namespace cml {

// Gamma family -----------------------------------------------------------
double gamma(double x);     // Lanczos approximation, reflection for x < 0.5
double lgamma(double x);    // log|Gamma(x)|
double beta(double a, double b);
double digamma(double x);

// Error function family ----------------------------------------------------
double erf(double x);
double erfc(double x);
double normal_pdf(double x, double mean = 0.0, double stddev = 1.0);
double normal_cdf(double x, double mean = 0.0, double stddev = 1.0);
double normal_quantile(double p, double mean = 0.0, double stddev = 1.0); // inverse CDF

// Combinatorics ------------------------------------------------------------
double factorial(double n);            // Gamma-based; exact for n <= 22, inf beyond 170
bool   factorial_exact(unsigned n, std::uint64_t* out); // false on overflow (n > 20)
double binomial(double n, double k);   // n choose k (real-valued via lgamma, exact for small ints)
double permutations(double n, double k);
std::uint64_t fibonacci(unsigned n);   // n <= 93, throws std::overflow_error beyond

// Number theory ------------------------------------------------------------
std::uint64_t gcd(std::uint64_t a, std::uint64_t b);
std::uint64_t lcm(std::uint64_t a, std::uint64_t b);  // throws std::overflow_error on overflow
bool is_prime(std::uint64_t n);
std::vector<std::uint64_t> prime_factors(std::uint64_t n);
std::vector<std::uint64_t> primes_up_to(std::uint64_t n); // sieve, capped at 10^7

// Numerical differentiation -----------------------------------------------
// Central difference with Richardson extrapolation; f is any callable.
template <typename F>
double derivative(F&& f, double x, double h = 1e-4) {
    const double d1 = (f(x + h) - f(x - h)) / (2.0 * h);
    const double d2 = (f(x + h / 2.0) - f(x - h / 2.0)) / h;
    return (4.0 * d2 - d1) / 3.0;
}
template <typename F>
double second_derivative(F&& f, double x, double h = 1e-3) {
    return (f(x + h) - 2.0 * f(x) + f(x - h)) / (h * h);
}

} // namespace cml

#endif // CML_SPECIAL_HPP
