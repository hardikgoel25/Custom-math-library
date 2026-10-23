// cml/special.cpp — special functions and number theory.
#include "cml/special.hpp"
#include "cml/core.hpp"

#include <stdexcept>

namespace cml {

namespace {
// Lanczos approximation coefficients (g = 7, n = 9), accurate to ~1e-15.
const double LANCZOS_G = 7.0;
const double LANCZOS[9] = {
    0.99999999999980993,
    676.5203681218851,
    -1259.1392167224028,
    771.32342877765313,
    -176.61502916214059,
    12.507343278686905,
    -0.13857109526572012,
    9.9843695780195716e-6,
    1.5056327351493116e-7,
};

double lanczos_sum(double x) { // x already shifted: expects x >= 0.5
    double a = LANCZOS[0];
    for (int i = 1; i < 9; ++i) a += LANCZOS[i] / (x + static_cast<double>(i));
    return a;
}
} // namespace

double gamma(double x) {
    if (is_nan(x)) return x;
    if (x == inf()) return x;
    if (x == -inf()) return nan();
    if (is_integer(x) && x <= 0.0) return nan();      // poles
    if (x < 0.5) {
        // Reflection: Gamma(x) Gamma(1-x) = pi / sin(pi x)
        return constants::pi / (sin(constants::pi * x) * gamma(1.0 - x));
    }
    if (x > 171.7) return inf();
    if (is_integer(x) && x <= 23.0) {                   // exact small factorials
        double r = 1.0;
        for (double k = 2.0; k < x; k += 1.0) r *= k;
        return r;
    }
    if (x > 25.0) {
        // Gamma(x) = (x-1)(x-2)...(x-k) Gamma(x-k): keeps the Lanczos power term
        // small so its exp(y ln t) rounding does not dominate.
        double product = 1.0;
        while (x > 25.0) { x -= 1.0; product *= x; }
        return product * gamma(x);
    }
    const double xx = x - 1.0;
    const double t = xx + LANCZOS_G + 0.5;
    return sqrt(constants::tau) * pow(t, xx + 0.5) * exp(-t) * lanczos_sum(xx);
}

double lgamma(double x) {
    if (is_nan(x)) return x;
    if (is_inf(x)) return inf();
    if (is_integer(x) && x <= 0.0) return inf();
    if (x < 0.5) {
        return ln(constants::pi / abs(sin(constants::pi * x))) - lgamma(1.0 - x);
    }
    const double xx = x - 1.0;
    const double t = xx + LANCZOS_G + 0.5;
    return 0.5 * ln(constants::tau) + (xx + 0.5) * ln(t) - t + ln(lanczos_sum(xx));
}

double beta(double a, double b) {
    if (a <= 0.0 || b <= 0.0) return nan();
    return exp(lgamma(a) + lgamma(b) - lgamma(a + b));
}

double digamma(double x) {
    if (is_nan(x)) return x;
    if (is_integer(x) && x <= 0.0) return nan();
    double result = 0.0;
    if (x < 0.0) {
        // Reflection: psi(1-x) - psi(x) = pi cot(pi x)
        return digamma(1.0 - x) - constants::pi / tan(constants::pi * x);
    }
    while (x < 30.0) {      // recurrence psi(x) = psi(x+1) - 1/x
        result -= 1.0 / x;
        x += 1.0;
    }
    const double f = 1.0 / (x * x);   // asymptotic expansion (Bernoulli numbers)
    result += ln(x) - 0.5 / x
              - f * (1.0 / 12.0 - f * (1.0 / 120.0 - f * (1.0 / 252.0 - f * (1.0 / 240.0 - f * (1.0 / 132.0 - f * (691.0 / 32760.0 - f / 12.0))))));
    return result;
}

// erf via the all-positive-term series 2/sqrt(pi) * exp(-x^2) * sum 2^n x^(2n+1) / (2n+1)!!
// (no cancellation), switching to the erfc continued fraction for |x| >= 3.
double erf(double x) {
    if (is_nan(x)) return x;
    if (is_inf(x)) return x > 0 ? 1.0 : -1.0;
    const double a = abs(x);
    if (a < 3.0) {
        const double z = 2.0 * a * a;
        double term = a;
        double sum = a;
        for (int n = 1; n < 400; ++n) {
            term *= z / static_cast<double>(2 * n + 1);
            sum += term;
            if (term < 1e-17 * sum) break;
        }
        const double r = sum * 2.0 / sqrt(constants::pi) * exp(-a * a);
        return x < 0 ? -r : r;
    }
    const double r = 1.0 - erfc(a);
    return x < 0 ? -r : r;
}

double erfc(double x) {
    if (is_nan(x)) return x;
    if (is_inf(x)) return x > 0 ? 0.0 : 2.0;
    if (x < 1.0) return 1.0 - erf(x);
    // Lentz continued fraction for erfc(x) = exp(-x^2)/(x sqrt(pi)) * 1/(1+ 1/(2x^2)/(1+ 2/(2x^2)/(1+...)))
    // Use the standard form: erfc(x) = exp(-x^2)/sqrt(pi) * 1/(x + 1/2/(x + 1/(x + 3/2/(x + 2/(x + ...)))))
    const double tiny = 1e-300;
    double f = x;
    double C = x;
    double D = 0.0;
    for (int n = 1; n < 300; ++n) {
        const double an = static_cast<double>(n) * 0.5;
        D = x + an * D;
        if (abs(D) < tiny) D = tiny;
        C = x + an / C;
        if (abs(C) < tiny) C = tiny;
        D = 1.0 / D;
        const double delta = C * D;
        f *= delta;
        if (abs(delta - 1.0) < 1e-16) break;
    }
    return exp(-x * x) / (sqrt(constants::pi) * f);
}

double normal_pdf(double x, double mean, double stddev) {
    if (stddev <= 0.0) return nan();
    const double z = (x - mean) / stddev;
    return exp(-0.5 * z * z) / (stddev * sqrt(constants::tau));
}

double normal_cdf(double x, double mean, double stddev) {
    if (stddev <= 0.0) return nan();
    const double z = (x - mean) / (stddev * constants::sqrt2);
    return 0.5 * erfc(-z);
}

double normal_quantile(double p, double mean, double stddev) {
    if (is_nan(p) || p < 0.0 || p > 1.0 || stddev <= 0.0) return nan();
    if (p == 0.0) return -inf();
    if (p == 1.0) return inf();
    // Acklam's rational approximation followed by one Newton refinement.
    static const double a[] = {-3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02,
                               1.383577518672690e+02, -3.066479806614716e+01, 2.506628277459239e+00};
    static const double b[] = {-5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02,
                               6.680131188771972e+01, -1.328068155288572e+01};
    static const double c[] = {-7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
                               -2.549732539343734e+00, 4.374664141464968e+00, 2.938163982698783e+00};
    static const double d[] = {7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00,
                               3.754408661907416e+00};
    const double plow = 0.02425, phigh = 1.0 - plow;
    double z;
    if (p < plow) {
        const double q = sqrt(-2.0 * ln(p));
        z = (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
            ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    } else if (p <= phigh) {
        const double q = p - 0.5, r = q * q;
        z = (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q /
            (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1.0);
    } else {
        const double q = sqrt(-2.0 * ln(1.0 - p));
        z = -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
            ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    }
    // Newton step on the standard normal CDF.
    for (int i = 0; i < 2; ++i) {
        const double e = 0.5 * erfc(-z / constants::sqrt2) - p;
        const double u = e * sqrt(constants::tau) * exp(z * z / 2.0);
        z = z - u / (1.0 + z * u / 2.0);
    }
    return mean + stddev * z;
}

double factorial(double n) {
    if (is_nan(n) || n < 0.0) return nan();
    if (n > 170.0) return inf();
    if (is_integer(n)) {
        double r = 1.0;
        for (double k = 2.0; k <= n; k += 1.0) r *= k;
        return r;
    }
    return gamma(n + 1.0);
}

bool factorial_exact(unsigned n, std::uint64_t* out) {
    if (n > 20) return false;
    std::uint64_t r = 1;
    for (unsigned k = 2; k <= n; ++k) r *= k;
    *out = r;
    return true;
}

double binomial(double n, double k) {
    if (is_nan(n) || is_nan(k)) return nan();
    if (is_integer(n) && is_integer(k)) {
        if (k < 0.0 || k > n) return 0.0;
        if (n <= 1000.0) {
            // Multiplicative formula keeps intermediate values exact as long as possible.
            double kk = k;
            if (kk > n - kk) kk = n - kk;
            double r = 1.0;
            for (double i = 1.0; i <= kk; i += 1.0) r = r * (n - kk + i) / i;
            return round(r);
        }
    }
    if (n < 0.0 || k < 0.0 || k > n) return nan();
    return exp(lgamma(n + 1.0) - lgamma(k + 1.0) - lgamma(n - k + 1.0));
}

double permutations(double n, double k) {
    if (is_nan(n) || is_nan(k)) return nan();
    if (k < 0.0 || k > n || n < 0.0) return 0.0;
    if (is_integer(n) && is_integer(k) && n <= 170.0) {
        double r = 1.0;
        for (double i = 0.0; i < k; i += 1.0) r *= (n - i);
        return r;
    }
    return exp(lgamma(n + 1.0) - lgamma(n - k + 1.0));
}

std::uint64_t fibonacci(unsigned n) {
    if (n > 93) throw std::overflow_error("fibonacci(n) overflows uint64 for n > 93");
    std::uint64_t a = 0, b = 1;
    for (unsigned i = 0; i < n; ++i) {
        const std::uint64_t t = a + b;
        a = b;
        b = t;
    }
    return a;
}

std::uint64_t gcd(std::uint64_t a, std::uint64_t b) {
    while (b != 0) {
        const std::uint64_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

std::uint64_t lcm(std::uint64_t a, std::uint64_t b) {
    if (a == 0 || b == 0) return 0;
    const std::uint64_t g = gcd(a, b);
    const std::uint64_t q = a / g;
    if (q > UINT64_C(0xFFFFFFFFFFFFFFFF) / b) throw std::overflow_error("lcm overflows uint64");
    return q * b;
}

namespace {
std::uint64_t mulmod(std::uint64_t a, std::uint64_t b, std::uint64_t m) {
#if defined(__SIZEOF_INT128__)
    return static_cast<std::uint64_t>((static_cast<unsigned __int128>(a) * b) % m);
#else
    std::uint64_t result = 0;
    a %= m;
    while (b > 0) {
        if (b & 1) result = (result >= m - a) ? result - (m - a) : result + a;
        a = (a >= m - a) ? a - (m - a) : a + a;
        b >>= 1;
    }
    return result;
#endif
}
std::uint64_t powmod(std::uint64_t base, std::uint64_t exp, std::uint64_t m) {
    std::uint64_t r = 1;
    base %= m;
    while (exp > 0) {
        if (exp & 1) r = mulmod(r, base, m);
        base = mulmod(base, base, m);
        exp >>= 1;
    }
    return r;
}
} // namespace

// Deterministic Miller–Rabin, exact for all 64-bit integers.
bool is_prime(std::uint64_t n) {
    if (n < 2) return false;
    static const std::uint64_t small[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
    for (std::uint64_t p : small) {
        if (n == p) return true;
        if (n % p == 0) return false;
    }
    std::uint64_t d = n - 1;
    int r = 0;
    while ((d & 1) == 0) { d >>= 1; ++r; }
    for (std::uint64_t a : small) {
        std::uint64_t x = powmod(a, d, n);
        if (x == 1 || x == n - 1) continue;
        bool composite = true;
        for (int i = 1; i < r; ++i) {
            x = mulmod(x, x, n);
            if (x == n - 1) { composite = false; break; }
        }
        if (composite) return false;
    }
    return true;
}

std::vector<std::uint64_t> prime_factors(std::uint64_t n) {
    std::vector<std::uint64_t> out;
    if (n < 2) return out;
    while ((n & 1) == 0) { out.push_back(2); n >>= 1; }
    for (std::uint64_t p = 3; p * p <= n; p += 2) {
        while (n % p == 0) { out.push_back(p); n /= p; }
        if (is_prime(n)) break;
    }
    if (n > 1) out.push_back(n);
    return out;
}

std::vector<std::uint64_t> primes_up_to(std::uint64_t n) {
    std::vector<std::uint64_t> out;
    if (n < 2) return out;
    if (n > 10000000ULL) throw std::length_error("primes_up_to limit is 10^7");
    std::vector<bool> sieve(static_cast<std::size_t>(n + 1), true);
    sieve[0] = sieve[1] = false;
    for (std::uint64_t i = 2; i * i <= n; ++i)
        if (sieve[static_cast<std::size_t>(i)])
            for (std::uint64_t j = i * i; j <= n; j += i) sieve[static_cast<std::size_t>(j)] = false;
    for (std::uint64_t i = 2; i <= n; ++i)
        if (sieve[static_cast<std::size_t>(i)]) out.push_back(i);
    return out;
}

} // namespace cml
