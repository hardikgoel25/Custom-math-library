// tests/test_core.cpp — elementary and special functions versus mpmath and <cmath>.
#include "cml_test.hpp"
#include "cml/core.hpp"
#include "cml/special.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <string>

namespace {

struct RefCase {
    const char* fn;
    double x;
    double expected;
};
#include "core_reference.inc"

using Fn1 = double (*)(double);
const std::map<std::string, Fn1>& function_table() {
    static const std::map<std::string, Fn1> t = {
        {"sin", cml::sin},     {"cos", cml::cos},     {"tan", cml::tan},       {"exp", cml::exp},
        {"ln", cml::ln},       {"sqrt", cml::sqrt},   {"cbrt", cml::cbrt},     {"atan", cml::atan},
        {"asin", cml::asin},   {"acos", cml::acos},   {"sinh", cml::sinh},     {"cosh", cml::cosh},
        {"tanh", cml::tanh},   {"asinh", cml::asinh}, {"acosh", cml::acosh},   {"atanh", cml::atanh},
        {"gamma", cml::gamma}, {"lgamma", cml::lgamma}, {"erf", cml::erf},     {"erfc", cml::erfc},
        {"digamma", cml::digamma}, {"log1p", cml::log1p}, {"expm1", cml::expm1},
    };
    return t;
}

// Per-function tolerance (relative). Most are a few ULP; the ones that chain
// several operations (gamma, digamma, trig at 1e15) get a little more room.
double tolerance_for(const std::string& fn, double x) {
    if (fn == "gamma" || fn == "lgamma" || fn == "digamma" || fn == "erfc") return 4e-14;
    if ((fn == "sin" || fn == "cos" || fn == "tan") && std::fabs(x) > 1e9) return 1e-13;
    if (fn == "tan") return 4e-15;
    return 1.5e-15;
}

const double NaN = std::numeric_limits<double>::quiet_NaN();
const double Inf = std::numeric_limits<double>::infinity();

} // namespace

TEST_CASE("core: reference values from mpmath") {
    double worst_ulps = 0.0;
    const char* worst = "";
    for (const RefCase& c : REF_CASES) {
        const Fn1 f = function_table().at(c.fn);
        const double got = f(c.x);
        const double rel = tolerance_for(c.fn, c.x);
        ++cmltest::state().checks;
        if (!cmltest::close(got, c.expected, rel, 0.0)) {
            cmltest::report_failure(__FILE__, __LINE__,
                                    std::string(c.fn) + "(" + cmltest::show(c.x) + ") = " + cmltest::show(got) +
                                        ", expected " + cmltest::show(c.expected) +
                                        " (ulps=" + cmltest::show(cmltest::ulps(got, c.expected)) + ")");
        }
        const double u = cmltest::ulps(got, c.expected);
        if (u > worst_ulps && std::isfinite(u)) { worst_ulps = u; worst = c.fn; }
    }
    std::printf("  worst case: %s at %.1f ulps\n", worst, worst_ulps);
}

TEST_CASE("core: sweep against <cmath> on dense grids") {
    // These grids catch range-reduction bugs that spot checks miss.
    for (int i = -2000; i <= 2000; ++i) {
        const double x = i * 0.01371;
        CHECK_CLOSE(cml::sin(x), std::sin(x), 4e-15);
        CHECK_CLOSE(cml::cos(x), std::cos(x), 4e-15);
        CHECK_CLOSE(cml::exp(x * 0.1), std::exp(x * 0.1), 2e-15);
        CHECK_CLOSE(cml::atan(x), std::atan(x), 2e-15);
        CHECK_CLOSE(cml::sinh(x * 0.1), std::sinh(x * 0.1), 4e-15);
        CHECK_CLOSE(cml::tanh(x * 0.1), std::tanh(x * 0.1), 4e-15);
        CHECK_CLOSE(cml::asinh(x), std::asinh(x), 4e-15);
    }
    for (int i = 1; i <= 2000; ++i) {
        const double x = i * 0.37;
        CHECK_CLOSE(cml::ln(x), std::log(x), 2e-15);
        CHECK_CLOSE(cml::sqrt(x), std::sqrt(x), 1e-15);
        CHECK_CLOSE(cml::cbrt(x), std::cbrt(x), 2e-15);
        CHECK_CLOSE(cml::log10(x), std::log10(x), 4e-15);
        CHECK_CLOSE(cml::log2(x), std::log2(x), 4e-15);
    }
    for (int i = -999; i <= 999; ++i) {
        const double x = i * 0.001;
        CHECK_CLOSE(cml::asin(x), std::asin(x), 4e-15);
        CHECK_CLOSE(cml::acos(x), std::acos(x), 4e-15);
        CHECK_CLOSE(cml::atanh(x), std::atanh(x), 4e-15);
        CHECK_CLOSE(cml::erf(x * 4), std::erf(x * 4), 4e-15);
    }
}

TEST_CASE("core: exact values and identities") {
    CHECK_EQ(cml::sqrt(4.0), 2.0);
    CHECK_EQ(cml::sqrt(1e100), 1e50);
    CHECK_EQ(cml::cbrt(27.0), 3.0);
    CHECK_EQ(cml::cbrt(-8.0), -2.0);
    CHECK_EQ(cml::exp(0.0), 1.0);
    CHECK_EQ(cml::ln(1.0), 0.0);
    CHECK_EQ(cml::log2(1024.0), 10.0);
    CHECK_EQ(cml::log2(0.25), -2.0);
    CHECK_EQ(cml::log10(1000.0), 3.0);
    CHECK_EQ(cml::log10(1e-7), -7.0);
    CHECK_EQ(cml::ipow(2.0, 10), 1024.0);
    CHECK_EQ(cml::ipow(2.0, -2), 0.25);
    CHECK_EQ(cml::ipow(-3.0, 3), -27.0);
    CHECK_EQ(cml::pow(2.0, 0.5), cml::sqrt(2.0));
    CHECK_CLOSE(cml::pow(2.0, 1.5), std::pow(2.0, 1.5), 2e-15);
    CHECK_CLOSE(cml::pow(10.0, -3.7), std::pow(10.0, -3.7), 4e-15);
    CHECK_EQ(cml::pow(-2.0, 3.0), -8.0);
    CHECK(cml::is_nan(cml::pow(-2.0, 0.5)));
    CHECK_EQ(cml::sin(0.0), 0.0);
    CHECK_EQ(cml::cos(0.0), 1.0);
    CHECK_CLOSE_ABS(cml::sin(cml::constants::pi), 0.0, 2e-16);
    CHECK_CLOSE_ABS(cml::cos(cml::constants::half_pi), 0.0, 2e-16);
    CHECK_CLOSE(cml::sin(cml::constants::half_pi), 1.0, 1e-16);
    CHECK_CLOSE(cml::tan(cml::constants::quarter_pi), 1.0, 4e-16);
    CHECK_CLOSE(cml::atan(1.0), cml::constants::quarter_pi, 1e-16);
    CHECK_CLOSE(cml::atan2(1.0, 1.0), cml::constants::quarter_pi, 1e-16);
    CHECK_CLOSE(cml::atan2(1.0, -1.0), 3 * cml::constants::quarter_pi, 1e-16);
    CHECK_CLOSE(cml::atan2(-1.0, -1.0), -3 * cml::constants::quarter_pi, 1e-16);
    CHECK_EQ(cml::atan2(0.0, -1.0), cml::constants::pi);
    CHECK_EQ(cml::atan2(-0.0, -1.0), -cml::constants::pi);
    CHECK_EQ(cml::atan2(0.0, 1.0), 0.0);
    CHECK_EQ(cml::acos(1.0), 0.0);
    CHECK_EQ(cml::acos(-1.0), cml::constants::pi);
    CHECK_EQ(cml::asin(1.0), cml::constants::half_pi);
    CHECK_EQ(cml::hypot(3.0, 4.0), 5.0);
    CHECK_CLOSE(cml::hypot(1e200, 1e200), 1.4142135623730951e200, 1e-15);
    CHECK_CLOSE(cml::nth_root(32.0, 5), 2.0, 1e-15);
    CHECK_CLOSE(cml::nth_root(-32.0, 5), -2.0, 1e-15);
    CHECK_CLOSE(cml::exp2(10.5), std::exp2(10.5), 2e-15);
    CHECK_EQ(cml::exp2(-3.0), 0.125);
    CHECK_CLOSE(cml::log_base(3.0, 81.0), 4.0, 4e-15);
    CHECK_CLOSE(cml::cot(1.0), 1.0 / std::tan(1.0), 4e-15);
    CHECK_CLOSE(cml::sec(1.0), 1.0 / std::cos(1.0), 4e-15);
    CHECK_CLOSE(cml::csc(1.0), 1.0 / std::sin(1.0), 4e-15);
    CHECK_CLOSE(cml::acot(2.0), std::atan(0.5), 2e-15);
    CHECK_CLOSE(cml::asec(2.0), std::acos(0.5), 2e-15);
    CHECK_CLOSE(cml::acsc(2.0), std::asin(0.5), 2e-15);
    CHECK_CLOSE(cml::coth(1.0), 1.0 / std::tanh(1.0), 4e-15);
    CHECK_CLOSE(cml::sech(1.0), 1.0 / std::cosh(1.0), 4e-15);
    CHECK_CLOSE(cml::csch(1.0), 1.0 / std::sinh(1.0), 4e-15);
    CHECK_CLOSE(cml::acoth(2.0), std::atanh(0.5), 2e-15);
    CHECK_CLOSE(cml::asech(0.5), std::acosh(2.0), 2e-15);
    CHECK_CLOSE(cml::acsch(0.5), std::asinh(2.0), 2e-15);
    CHECK_CLOSE(cml::deg2rad(180.0), cml::constants::pi, 1e-16);
    CHECK_CLOSE(cml::rad2deg(cml::constants::pi), 180.0, 1e-16);
}

TEST_CASE("core: rounding, fmod, frexp, ldexp") {
    CHECK_EQ(cml::floor(2.7), 2.0);
    CHECK_EQ(cml::floor(-2.7), -3.0);
    CHECK_EQ(cml::floor(-2.0), -2.0);
    CHECK_EQ(cml::ceil(2.1), 3.0);
    CHECK_EQ(cml::ceil(-2.1), -2.0);
    CHECK_EQ(cml::trunc(-2.9), -2.0);
    CHECK_EQ(cml::round(2.5), 3.0);
    CHECK_EQ(cml::round(-2.5), -3.0);
    CHECK_EQ(cml::round(2.4999999999999996), 2.0);
    CHECK_EQ(cml::round(0.49999999999999994), 0.0);
    CHECK_EQ(cml::floor(1e300), 1e300);
    CHECK(cml::sign_bit(cml::floor(-0.5)) && cml::floor(-0.5) == -1.0);
    CHECK(cml::sign_bit(cml::ceil(-0.5)));   // -0.0
    for (int i = -500; i <= 500; ++i) {
        const double x = i * 0.731;
        for (int j = 1; j <= 7; ++j) {
            const double y = j * 0.37;
            CHECK_EQ(cml::fmod(x, y), std::fmod(x, y));
            CHECK_EQ(cml::fmod(x, -y), std::fmod(x, -y));
        }
    }
    CHECK_EQ(cml::fmod(1e300, 3.0), std::fmod(1e300, 3.0));
    CHECK_EQ(cml::fmod(5.5, 1e-300), std::fmod(5.5, 1e-300));
    CHECK(cml::is_nan(cml::fmod(1.0, 0.0)));
    CHECK(cml::is_nan(cml::fmod(Inf, 2.0)));
    int e1, e2;
    const double xs[] = {1.0, 0.1, 123456.789, 1e-310, 1e300, -3.5};
    for (double x : xs) {
        const double m1 = cml::frexp(x, &e1);
        const double m2 = std::frexp(x, &e2);
        CHECK_EQ(m1, m2);
        CHECK_EQ(e1, e2);
    }
    CHECK_EQ(cml::ldexp(1.0, 10), 1024.0);
    CHECK_EQ(cml::ldexp(1.0, -1074), std::ldexp(1.0, -1074));
    CHECK_EQ(cml::ldexp(1.5, 1023), std::ldexp(1.5, 1023));
    CHECK_EQ(cml::ldexp(1.0, 2000), Inf);
    CHECK_EQ(cml::ldexp(1.0, -2000), 0.0);
    CHECK_EQ(cml::ldexp(3.0, -1060), std::ldexp(3.0, -1060));
}

TEST_CASE("core: IEEE special cases") {
    CHECK(cml::is_nan(cml::nan()));
    CHECK(cml::is_inf(cml::inf()));
    CHECK(!cml::is_finite(cml::inf()));
    CHECK(cml::is_finite(1e308));
    CHECK(cml::is_nan(cml::sqrt(-1.0)));
    CHECK_EQ(cml::sqrt(Inf), Inf);
    CHECK_EQ(cml::ln(0.0), -Inf);
    CHECK(cml::is_nan(cml::ln(-1.0)));
    CHECK_EQ(cml::ln(Inf), Inf);
    CHECK_EQ(cml::exp(Inf), Inf);
    CHECK_EQ(cml::exp(-Inf), 0.0);
    CHECK_EQ(cml::exp(1000.0), Inf);
    CHECK_EQ(cml::exp(-1000.0), 0.0);
    CHECK(cml::is_nan(cml::sin(Inf)));
    CHECK(cml::is_nan(cml::cos(NaN)));
    CHECK_EQ(cml::atan(Inf), cml::constants::half_pi);
    CHECK_EQ(cml::atan(-Inf), -cml::constants::half_pi);
    CHECK(cml::is_nan(cml::asin(1.5)));
    CHECK(cml::is_nan(cml::acosh(0.5)));
    CHECK_EQ(cml::atanh(1.0), Inf);
    CHECK_EQ(cml::atanh(-1.0), -Inf);
    CHECK_EQ(cml::tanh(1000.0), 1.0);
    CHECK_EQ(cml::tanh(-1000.0), -1.0);
    CHECK_EQ(cml::sinh(1000.0), Inf);
    CHECK_EQ(cml::cosh(-1000.0), Inf);
    // pow special cases (C99 Annex F)
    CHECK_EQ(cml::pow(NaN, 0.0), 1.0);
    CHECK_EQ(cml::pow(1.0, NaN), 1.0);
    CHECK(cml::is_nan(cml::pow(NaN, 1.0)));
    CHECK_EQ(cml::pow(0.0, -1.0), Inf);
    CHECK_EQ(cml::pow(-0.0, -1.0), -Inf);
    CHECK_EQ(cml::pow(-0.0, -2.0), Inf);
    CHECK_EQ(cml::pow(0.0, 3.0), 0.0);
    CHECK(cml::sign_bit(cml::pow(-0.0, 3.0)));
    CHECK_EQ(cml::pow(-1.0, Inf), 1.0);
    CHECK_EQ(cml::pow(0.5, -Inf), Inf);
    CHECK_EQ(cml::pow(2.0, -Inf), 0.0);
    CHECK_EQ(cml::pow(0.5, Inf), 0.0);
    CHECK_EQ(cml::pow(2.0, Inf), Inf);
    CHECK_EQ(cml::pow(-Inf, 3.0), -Inf);
    CHECK_EQ(cml::pow(-Inf, 2.0), Inf);
    CHECK_EQ(cml::pow(-Inf, -3.0), -0.0);
    CHECK(cml::sign_bit(cml::pow(-Inf, -3.0)));
    CHECK_EQ(cml::pow(Inf, -1.0), 0.0);
    CHECK_EQ(cml::pow(Inf, 1.0), Inf);
    CHECK_EQ(cml::pow(2.0, 1e20), Inf);
    CHECK_EQ(cml::pow(2.0, -1e20), 0.0);
    CHECK_EQ(cml::fmax(NaN, 1.0), 1.0);
    CHECK_EQ(cml::fmin(1.0, NaN), 1.0);
    CHECK_EQ(cml::copysign(3.0, -0.0), -3.0);
    CHECK_EQ(cml::abs(-0.0), 0.0);
    CHECK(!cml::sign_bit(cml::abs(-0.0)));
    CHECK_EQ(cml::sign(-5.0), -1);
    CHECK_EQ(cml::sign(0.0), 0);
    CHECK_EQ(cml::sign(NaN), 0);
    CHECK(cml::is_integer(4.0));
    CHECK(!cml::is_integer(4.5));
    CHECK(!cml::is_integer(Inf));
    CHECK(cml::nearly_equal(1.0, 1.0 + 1e-13));
    CHECK(!cml::nearly_equal(1.0, 1.1));
    CHECK_EQ(cml::to_string(1.0 / 3.0, 6), std::string("0.333333"));
    CHECK_EQ(cml::to_string(NaN), std::string("NaN"));
    CHECK_EQ(cml::to_string(-Inf), std::string("-Infinity"));
}

TEST_CASE("special: combinatorics and number theory") {
    CHECK_EQ(cml::factorial(0.0), 1.0);
    CHECK_EQ(cml::factorial(5.0), 120.0);
    CHECK_EQ(cml::factorial(20.0), 2432902008176640000.0);
    CHECK_EQ(cml::factorial(171.0), Inf);
    CHECK(cml::is_nan(cml::factorial(-1.0)));
    CHECK_CLOSE(cml::factorial(0.5), cml::gamma(1.5), 1e-15);
    std::uint64_t f;
    CHECK(cml::factorial_exact(20, &f));
    CHECK_EQ(f, 2432902008176640000ULL);
    CHECK(!cml::factorial_exact(21, &f));
    CHECK_EQ(cml::binomial(10.0, 3.0), 120.0);
    CHECK_EQ(cml::binomial(52.0, 5.0), 2598960.0);
    CHECK_EQ(cml::binomial(5.0, 7.0), 0.0);
    CHECK_EQ(cml::binomial(60.0, 30.0), 118264581564861424.0);
    CHECK_EQ(cml::permutations(10.0, 3.0), 720.0);
    CHECK_EQ(cml::fibonacci(0), 0ULL);
    CHECK_EQ(cml::fibonacci(1), 1ULL);
    CHECK_EQ(cml::fibonacci(10), 55ULL);
    CHECK_EQ(cml::fibonacci(93), 12200160415121876738ULL);
    CHECK_THROWS(cml::fibonacci(94), std::overflow_error);
    CHECK_EQ(cml::gcd(48, 18), 6ULL);
    CHECK_EQ(cml::gcd(0, 5), 5ULL);
    CHECK_EQ(cml::lcm(4, 6), 12ULL);
    CHECK_THROWS(cml::lcm(1ULL << 40, (1ULL << 40) + 1), std::overflow_error);
    CHECK(cml::is_prime(2));
    CHECK(cml::is_prime(97));
    CHECK(!cml::is_prime(1));
    CHECK(!cml::is_prime(561));                    // Carmichael number
    CHECK(cml::is_prime(1000000007ULL));
    CHECK(cml::is_prime(18446744073709551557ULL)); // largest 64-bit prime
    CHECK(!cml::is_prime(18446744073709551557ULL - 2));
    const auto pf = cml::prime_factors(360);
    CHECK_EQ(pf.size(), 6u);
    CHECK_EQ(pf[0], 2ULL);
    CHECK_EQ(pf[5], 5ULL);
    const auto pf2 = cml::prime_factors(1000000007ULL * 3);
    CHECK_EQ(pf2.size(), 2u);
    CHECK_EQ(pf2[1], 1000000007ULL);
    const auto primes = cml::primes_up_to(30);
    CHECK_EQ(primes.size(), 10u);
    CHECK_EQ(primes.back(), 29ULL);
    CHECK_THROWS(cml::primes_up_to(100000000ULL), std::length_error);
    CHECK_CLOSE(cml::beta(2.0, 3.0), 1.0 / 12.0, 1e-13);
    CHECK_CLOSE(cml::normal_cdf(0.0), 0.5, 1e-15);
    CHECK_CLOSE(cml::normal_cdf(1.96), 0.9750021048517795, 1e-13);
    CHECK_CLOSE(cml::normal_pdf(0.0), 0.3989422804014327, 1e-15);
    CHECK_CLOSE(cml::normal_quantile(0.975), 1.959963984540054, 1e-12);
    CHECK_CLOSE(cml::normal_quantile(0.01), -2.3263478740408408, 1e-12);
    CHECK_CLOSE(cml::normal_quantile(0.5), 0.0, 1e-12);
    CHECK_CLOSE(cml::gamma(5.0), 24.0, 1e-15);
    CHECK(cml::is_nan(cml::gamma(-2.0)));
    CHECK_CLOSE(cml::derivative([](double x) { return x * x * x; }, 2.0), 12.0, 1e-9);
    CHECK_CLOSE(cml::second_derivative([](double x) { return x * x * x; }, 2.0), 12.0, 1e-6);
}
