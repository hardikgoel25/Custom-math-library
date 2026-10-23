// cml/core.cpp — elementary functions implemented from scratch.
#include "cml/core.hpp"

#include <cstdio>
#include <cstring>
#include <climits>

namespace cml {

// ---------------------------------------------------------------------------
// Bit-level helpers (IEEE-754 binary64)
// ---------------------------------------------------------------------------
namespace {

inline std::uint64_t bits_of(double x) {
    std::uint64_t u;
    std::memcpy(&u, &x, sizeof u);
    return u;
}
inline double from_bits(std::uint64_t u) {
    double x;
    std::memcpy(&x, &u, sizeof x);
    return x;
}

const double TWO_POW_52    = 4503599627370496.0;      // 2^52
const double TWO_POW_53    = 9007199254740992.0;      // 2^53
const double TWO_POW_1023  = 8.98846567431158e307;    // 2^1023
const double TWO_POW_M1022 = 2.2250738585072014e-308; // 2^-1022

// pi/2 split into three parts: the product n*PIO2_HI is computed exactly with
// TwoProd, the remaining parts contribute below the rounding level, so the
// reduction stays accurate for |x| up to roughly 1e15 (Cody-Waite style).
const double PIO2_HI  = 1.5707963267948966;    // 0x3FF921FB54442D18
const double PIO2_MID = 6.123233995736766e-17; // 0x3C91A62633145C07
const double PIO2_LO  = -1.4973849048591698e-33;

// ln(2) split (fdlibm)
const double LN2_HI = 6.93147180369123816490e-01;
const double LN2_LO = 1.90821492927058770002e-10;

// Veltkamp split: a = hi + lo exactly, hi has 26 significant bits.
inline void split(double a, double* hi, double* lo) {
    const double c = 134217729.0 * a; // 2^27 + 1
    *hi = c - (c - a);
    *lo = a - *hi;
}

// Exact product a*b = p + err (Dekker TwoProd without FMA).
inline double two_prod(double a, double b, double* err) {
    const double p = a * b;
    double ah, al, bh, bl;
    split(a, &ah, &al);
    split(b, &bh, &bl);
    *err = ((ah * bh - p) + ah * bl + al * bh) + al * bl;
    return p;
}

// Inverse factorial table (computed once, deterministic).
struct InvFactorials {
    double v[40];
    InvFactorials() {
        double f = 1.0;
        v[0] = 1.0;
        for (int i = 1; i < 40; ++i) {
            f *= static_cast<double>(i);
            v[i] = 1.0 / f;
        }
    }
};
const InvFactorials& inv_fact() {
    static const InvFactorials t;
    return t;
}

// Kernel sin on |r| <= pi/4 (Taylor series in Horner form, 12 terms).
inline double kernel_sin(double r) {
    const double z = r * r;
    const InvFactorials& f = inv_fact();
    double p = f.v[23];
    p = f.v[21] - z * p;
    p = f.v[19] - z * p;
    p = f.v[17] - z * p;
    p = f.v[15] - z * p;
    p = f.v[13] - z * p;
    p = f.v[11] - z * p;
    p = f.v[9]  - z * p;
    p = f.v[7]  - z * p;
    p = f.v[5]  - z * p;
    p = f.v[3]  - z * p;
    return r - r * z * p;
}

// Kernel cos on |r| <= pi/4.
inline double kernel_cos(double r) {
    const double z = r * r;
    const InvFactorials& f = inv_fact();
    double p = f.v[24];
    p = f.v[22] - z * p;
    p = f.v[20] - z * p;
    p = f.v[18] - z * p;
    p = f.v[16] - z * p;
    p = f.v[14] - z * p;
    p = f.v[12] - z * p;
    p = f.v[10] - z * p;
    p = f.v[8]  - z * p;
    p = f.v[6]  - z * p;
    p = f.v[4]  - z * p;
    const double hz = 0.5 * z;
    return (1.0 - hz) + z * z * p;
}

// Reduce x to r in [-pi/4, pi/4]; returns the quadrant (0..3).
inline int reduce_pio2(double x, double* r) {
    const double inv_pio2 = 0.6366197723675814; // 2/pi
    double fn = round(x * inv_pio2);
    if (fn == 0.0) {
        *r = x;
        return 0;
    }
    double err;
    const double p = two_prod(fn, PIO2_HI, &err);
    double rem = (x - p) - err;
    rem -= fn * PIO2_MID;
    rem -= fn * PIO2_LO;
    *r = rem;
    double q = fmod(fn, 4.0);
    if (q < 0) q += 4.0;
    return static_cast<int>(q);
}

// e^r for |r| <= ~0.35 via Horner-evaluated Taylor series (20 terms).
inline double exp_kernel(double r) {
    const InvFactorials& f = inv_fact();
    double p = f.v[20];
    for (int i = 19; i >= 1; --i) p = f.v[i] + r * p;
    return 1.0 + r * p;
}

} // namespace

// ---------------------------------------------------------------------------
// Classification
// ---------------------------------------------------------------------------
double nan() { return from_bits(0x7FF8000000000000ULL); }
double inf() { return from_bits(0x7FF0000000000000ULL); }
bool is_nan(double x) { return x != x; }
bool is_inf(double x) { return (bits_of(x) & 0x7FFFFFFFFFFFFFFFULL) == 0x7FF0000000000000ULL; }
bool is_finite(double x) { return ((bits_of(x) >> 52) & 0x7FFULL) != 0x7FFULL; }
bool is_integer(double x) { return is_finite(x) && floor(x) == x; }
int sign(double x) { return (x > 0.0) - (x < 0.0); }
bool sign_bit(double x) { return (bits_of(x) >> 63) != 0; }

// ---------------------------------------------------------------------------
// Basic helpers
// ---------------------------------------------------------------------------
double abs(double x) { return from_bits(bits_of(x) & 0x7FFFFFFFFFFFFFFFULL); }
double fmin(double a, double b) {
    if (is_nan(a)) return b;
    if (is_nan(b)) return a;
    return a < b ? a : b;
}
double fmax(double a, double b) {
    if (is_nan(a)) return b;
    if (is_nan(b)) return a;
    return a > b ? a : b;
}
double clamp(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
double copysign(double mag, double sgn) {
    return from_bits((bits_of(mag) & 0x7FFFFFFFFFFFFFFFULL) | (bits_of(sgn) & 0x8000000000000000ULL));
}

double trunc(double x) {
    if (!is_finite(x) || abs(x) >= TWO_POW_52) return x;
    if (x == 0.0) return x;
    const double r = static_cast<double>(static_cast<long long>(x));
    return r == 0.0 ? copysign(0.0, x) : r;
}
double floor(double x) {
    if (!is_finite(x) || abs(x) >= TWO_POW_52) return x;
    double r = trunc(x);
    if (r > x) r -= 1.0;
    return (r == 0.0) ? copysign(0.0, x) : r;
}
double ceil(double x) {
    if (!is_finite(x) || abs(x) >= TWO_POW_52) return x;
    double r = trunc(x);
    if (r < x) r += 1.0;
    return (r == 0.0) ? copysign(0.0, x) : r;
}
double round(double x) {
    if (!is_finite(x) || abs(x) >= TWO_POW_52) return x;
    double t = trunc(x);
    if (abs(x - t) >= 0.5) t += (x > 0 ? 1.0 : -1.0);
    return (t == 0.0) ? copysign(0.0, x) : t;
}

double ldexp(double x, int n) {
    if (n > 1023) {
        x *= TWO_POW_1023; n -= 1023;
        if (n > 1023) {
            x *= TWO_POW_1023; n -= 1023;
            if (n > 1023) n = 1023;
        }
    } else if (n < -1022) {
        x *= TWO_POW_M1022 * TWO_POW_53; n += 1022 - 53;
        if (n < -1022) {
            x *= TWO_POW_M1022 * TWO_POW_53; n += 1022 - 53;
            if (n < -1022) n = -1022;
        }
    }
    return x * from_bits(static_cast<std::uint64_t>(0x3FF + n) << 52);
}

double frexp(double x, int* e) {
    std::uint64_t u = bits_of(x);
    const int ee = static_cast<int>((u >> 52) & 0x7FF);
    if (ee == 0) {
        if (x != 0.0) {
            x = frexp(x * 18446744073709551616.0, e); // 2^64
            *e -= 64;
        } else {
            *e = 0;
        }
        return x;
    }
    if (ee == 0x7FF) { *e = 0; return x; }
    *e = ee - 0x3FE;
    u &= 0x800FFFFFFFFFFFFFULL;
    u |= 0x3FE0000000000000ULL;
    return from_bits(u);
}

// Exact fmod using integer arithmetic on the bit patterns (musl algorithm).
double fmod(double x, double y) {
    std::uint64_t uxi = bits_of(x);
    std::uint64_t uyi = bits_of(y);
    int ex = static_cast<int>((uxi >> 52) & 0x7FF);
    int ey = static_cast<int>((uyi >> 52) & 0x7FF);
    const std::uint64_t sx = uxi >> 63;
    std::uint64_t i;

    if ((uyi << 1) == 0 || is_nan(y) || ex == 0x7FF) return (x * y) / (x * y);
    if ((uxi << 1) <= (uyi << 1)) {
        if ((uxi << 1) == (uyi << 1)) return 0.0 * x;
        return x;
    }
    if (!ex) {
        for (i = uxi << 12; (i >> 63) == 0; ex--, i <<= 1) {}
        uxi <<= -ex + 1;
    } else {
        uxi &= ~0ULL >> 12;
        uxi |= 1ULL << 52;
    }
    if (!ey) {
        for (i = uyi << 12; (i >> 63) == 0; ey--, i <<= 1) {}
        uyi <<= -ey + 1;
    } else {
        uyi &= ~0ULL >> 12;
        uyi |= 1ULL << 52;
    }
    for (; ex > ey; ex--) {
        i = uxi - uyi;
        if ((i >> 63) == 0) {
            if (i == 0) return 0.0 * x;
            uxi = i;
        }
        uxi <<= 1;
    }
    i = uxi - uyi;
    if ((i >> 63) == 0) {
        if (i == 0) return 0.0 * x;
        uxi = i;
    }
    for (; (uxi >> 52) == 0; uxi <<= 1, ex--) {}
    if (ex > 0) {
        uxi -= 1ULL << 52;
        uxi |= static_cast<std::uint64_t>(ex) << 52;
    } else {
        uxi >>= -ex + 1;
    }
    uxi |= sx << 63;
    return from_bits(uxi);
}

bool nearly_equal(double a, double b, double rel, double abs_tol) {
    if (a == b) return true;
    if (is_nan(a) || is_nan(b)) return false;
    const double diff = abs(a - b);
    if (diff <= abs_tol) return true;
    const double scale = fmax(abs(a), abs(b));
    return diff <= rel * scale;
}
double deg2rad(double d) { return d * constants::rad_per_deg; }
double rad2deg(double r) { return r * constants::deg_per_rad; }

// ---------------------------------------------------------------------------
// Powers and roots
// ---------------------------------------------------------------------------
double ipow(double x, long long n) {
    if (n == 0) return 1.0;
    if (is_nan(x)) return x;
    if (n < 0) {
        if (n == LLONG_MIN) return 1.0 / (ipow(x, LLONG_MAX) * x);
        return 1.0 / ipow(x, -n);
    }
    double result = 1.0;
    double base = x;
    while (n > 0) {
        if (n & 1) result *= base;
        n >>= 1;
        if (n) base *= base;
    }
    return result;
}

double sqrt(double x) {
    if (is_nan(x)) return x;
    if (x < 0.0) return nan();
    if (x == 0.0 || is_inf(x)) return x;
    int e;
    double m = frexp(x, &e);           // m in [0.5, 1)
    if (e & 1) { m *= 2.0; e -= 1; }   // m in [0.5, 2), e even
    double y = 0.41731 + 0.59016 * m;  // linear seed, relative error < 3%
    for (int i = 0; i < 8; ++i) {
        const double ny = 0.5 * (y + m / y);
        if (ny == y) break;
        y = ny;
    }
    y = y - (y * y - m) / (2.0 * y);   // final correction step
    return ldexp(y, e / 2);
}

double cbrt(double x) {
    if (is_nan(x) || is_inf(x) || x == 0.0) return x;
    const bool neg = x < 0.0;
    const double a = abs(x);
    int e;
    double m = frexp(a, &e);           // a = m * 2^e
    int rem = e % 3;
    if (rem < 0) rem += 3;
    m = ldexp(m, rem);                 // m in [0.5, 4)
    e -= rem;                          // e divisible by 3
    double y = 0.6 + 0.4 * m;          // rough seed
    for (int i = 0; i < 40; ++i) {
        const double ny = (2.0 * y + m / (y * y)) / 3.0;
        if (abs(ny - y) <= 2.0 * constants::epsilon * abs(ny)) { y = ny; break; }
        y = ny;
    }
    y = y - (y * y * y - m) / (3.0 * y * y);
    const double r = ldexp(y, e / 3);
    return neg ? -r : r;
}

double hypot(double x, double y) {
    double a = abs(x), b = abs(y);
    if (is_inf(a) || is_inf(b)) return inf();
    if (is_nan(a) || is_nan(b)) return nan();
    if (a < b) { const double t = a; a = b; b = t; }
    if (a == 0.0) return 0.0;
    const double r = b / a;
    return a * sqrt(1.0 + r * r);
}

double pow(double x, double y) {
    // IEEE / C99 Annex F special cases.
    if (y == 0.0) return 1.0;
    if (x == 1.0) return 1.0;
    if (is_nan(x) || is_nan(y)) return nan();
    const bool y_int = is_integer(y);
    const bool y_odd = y_int && fmod(abs(y), 2.0) == 1.0;
    if (x == 0.0) {
        if (y < 0.0) return (y_odd && sign_bit(x)) ? -inf() : inf();
        return (y_odd && sign_bit(x)) ? -0.0 : 0.0;
    }
    if (is_inf(y)) {
        if (x == -1.0) return 1.0;
        const double ax = abs(x);
        if (y < 0.0) return ax < 1.0 ? inf() : 0.0;
        return ax < 1.0 ? 0.0 : inf();
    }
    if (is_inf(x)) {
        if (x < 0.0) {
            if (y < 0.0) return y_odd ? -0.0 : 0.0;
            return y_odd ? -inf() : inf();
        }
        return y < 0.0 ? 0.0 : inf();
    }
    if (x < 0.0) {
        if (!y_int) return nan();
        const double r = pow(-x, y);
        return y_odd ? -r : r;
    }
    if (y_int && abs(y) < TWO_POW_53) return ipow(x, static_cast<long long>(y));
    if (y == 0.5) return sqrt(x);
    return exp(y * ln(x));
}

double nth_root(double x, int n) {
    if (n == 0) return nan();
    if (n < 0) return 1.0 / nth_root(x, -n);
    if (n == 1) return x;
    if (n == 2) return sqrt(x);
    if (n == 3) return cbrt(x);
    if (is_nan(x)) return x;
    if (x < 0.0) {
        if (n % 2 == 0) return nan();
        return -nth_root(-x, n);
    }
    if (x == 0.0 || is_inf(x)) return x;
    double y = exp(ln(x) / n);
    for (int i = 0; i < 3; ++i) {
        const double yn1 = ipow(y, n - 1);
        y -= (yn1 * y - x) / (n * yn1);
    }
    return y;
}

// ---------------------------------------------------------------------------
// Exponentials and logarithms
// ---------------------------------------------------------------------------
double exp(double x) {
    if (is_nan(x)) return x;
    if (x > 709.782712893384) return inf();
    if (x < -745.1332191019412) return 0.0;
    if (x == 0.0) return 1.0;
    const double k = round(x * constants::log2e);
    const double r = (x - k * LN2_HI) - k * LN2_LO;
    return ldexp(exp_kernel(r), static_cast<int>(k));
}

double exp2(double x) {
    if (is_nan(x)) return x;
    if (x > 1024.0) return inf();
    if (x < -1075.0) return 0.0;
    const double k = round(x);
    const double f = x - k;
    return ldexp(exp_kernel(f * constants::ln2), static_cast<int>(k));
}

double expm1(double x) {
    if (is_nan(x)) return x;
    if (abs(x) < 0.35) {
        const InvFactorials& f = inv_fact();
        double p = f.v[20];
        for (int i = 19; i >= 1; --i) p = f.v[i] + x * p;
        return x * p;
    }
    return exp(x) - 1.0;
}

double ln(double x) {
    if (is_nan(x)) return x;
    if (x < 0.0) return nan();
    if (x == 0.0) return -inf();
    if (is_inf(x)) return x;
    if (x == 1.0) return 0.0;
    int e;
    double m = frexp(x, &e);                              // m in [0.5, 1)
    if (m < constants::inv_sqrt2) { m *= 2.0; e -= 1; }   // m in [0.7071, 1.4142)
    const double s = (m - 1.0) / (m + 1.0);               // |s| <= 0.1716
    const double z = s * s;
    double p = 1.0 / 29.0;                                // 2s(1 + z/3 + z^2/5 + ...)
    for (int k = 27; k >= 1; k -= 2) p = 1.0 / static_cast<double>(k) + z * p;
    const double lnm = 2.0 * s * p;
    const double ee = static_cast<double>(e);
    return ee * LN2_HI + (lnm + ee * LN2_LO);
}
double log(double x) { return ln(x); }

double log2(double x) {
    if (x > 0.0 && is_finite(x)) {
        int e;
        const double m = frexp(x, &e);
        if (m == 0.5) return static_cast<double>(e - 1); // exact power of two
    }
    return ln(x) * constants::log2e;
}

double log10(double x) {
    const double r = ln(x) * constants::log10e;
    if (is_finite(r)) {
        const double ri = round(r);
        if (abs(r - ri) < 1e-9 && ipow(10.0, static_cast<long long>(ri)) == x) return ri;
    }
    return r;
}

double log_base(double base, double x) {
    if (base <= 0.0 || base == 1.0) return nan();
    return ln(x) / ln(base);
}

double log1p(double x) {
    if (is_nan(x)) return x;
    if (x < -1.0) return nan();
    if (x == -1.0) return -inf();
    const double u = 1.0 + x;
    if (u == 1.0) return x;
    return ln(u) * x / (u - 1.0);
}

// ---------------------------------------------------------------------------
// Trigonometry
// ---------------------------------------------------------------------------
double sin(double x) {
    if (!is_finite(x)) return nan();
    if (abs(x) < 7.450580596923828e-9) return x; // |x| < 2^-27
    double r;
    const int q = reduce_pio2(x, &r);
    switch (q) {
        case 0: return kernel_sin(r);
        case 1: return kernel_cos(r);
        case 2: return -kernel_sin(r);
        default: return -kernel_cos(r);
    }
}

double cos(double x) {
    if (!is_finite(x)) return nan();
    double r;
    const int q = reduce_pio2(x, &r);
    switch (q) {
        case 0: return kernel_cos(r);
        case 1: return -kernel_sin(r);
        case 2: return -kernel_cos(r);
        default: return kernel_sin(r);
    }
}

void sincos(double x, double* s, double* c) {
    if (!is_finite(x)) { *s = *c = nan(); return; }
    double r;
    const int q = reduce_pio2(x, &r);
    const double ks = kernel_sin(r), kc = kernel_cos(r);
    switch (q) {
        case 0: *s = ks;  *c = kc;  break;
        case 1: *s = kc;  *c = -ks; break;
        case 2: *s = -ks; *c = -kc; break;
        default:*s = -kc; *c = ks;  break;
    }
}

double tan(double x) {
    double s, c;
    sincos(x, &s, &c);
    return s / c;
}
double csc(double x) { return 1.0 / sin(x); }
double sec(double x) { return 1.0 / cos(x); }
double cot(double x) {
    double s, c;
    sincos(x, &s, &c);
    return c / s;
}

double atan(double x) {
    if (is_nan(x)) return x;
    if (is_inf(x)) return copysign(constants::half_pi, x);
    if (x == 0.0) return x;
    const bool neg = x < 0.0;
    double a = abs(x);
    double offset = 0.0;
    bool invert = false;
    if (a > 1.0) { a = 1.0 / a; invert = true; }
    if (a > 0.2679491924311227) { // tan(pi/12)
        a = (a * constants::sqrt3 - 1.0) / (a + constants::sqrt3);
        offset = constants::pi / 6.0;
    }
    const double z = a * a;
    double term = a;
    double sum = a;
    for (int k = 1; k < 60; ++k) {
        term *= -z;
        const double contrib = term / static_cast<double>(2 * k + 1);
        sum += contrib;
        if (abs(contrib) < 1e-18 * abs(sum)) break;
    }
    double r = sum + offset;
    if (invert) r = constants::half_pi - r;
    return neg ? -r : r;
}

double asin(double x) {
    if (is_nan(x)) return x;
    const double a = abs(x);
    if (a > 1.0) return nan();
    if (a == 1.0) return copysign(constants::half_pi, x);
    if (a < 0.5) return atan(x / sqrt(1.0 - x * x));
    const double t = sqrt((1.0 - a) * 0.5);   // asin(a) = pi/2 - 2 asin(t)
    const double r = constants::half_pi - 2.0 * atan(t / sqrt(1.0 - t * t));
    return x < 0.0 ? -r : r;
}

double acos(double x) {
    if (is_nan(x)) return x;
    if (x > 1.0 || x < -1.0) return nan();
    if (x == 1.0) return 0.0;
    if (x == -1.0) return constants::pi;
    return 2.0 * atan(sqrt((1.0 - x) / (1.0 + x)));
}

double atan2(double y, double x) {
    if (is_nan(x) || is_nan(y)) return nan();
    if (is_inf(x)) {
        if (is_inf(y)) {
            const double q = x > 0 ? constants::quarter_pi : 3.0 * constants::quarter_pi;
            return copysign(q, y);
        }
        return x > 0 ? copysign(0.0, y) : copysign(constants::pi, y);
    }
    if (is_inf(y)) return copysign(constants::half_pi, y);
    if (x == 0.0) {
        if (y == 0.0) return sign_bit(x) ? copysign(constants::pi, y) : copysign(0.0, y);
        return copysign(constants::half_pi, y);
    }
    if (y == 0.0) return x > 0 ? copysign(0.0, y) : copysign(constants::pi, y);
    const double a = atan(y / x);
    if (x > 0.0) return a;
    return y >= 0.0 ? a + constants::pi : a - constants::pi;
}

double acsc(double x) { return asin(1.0 / x); }
double asec(double x) { return acos(1.0 / x); }
double acot(double x) {
    if (x == 0.0) return constants::half_pi;
    return atan(1.0 / x);
}

// ---------------------------------------------------------------------------
// Hyperbolic
// ---------------------------------------------------------------------------
double sinh(double x) {
    if (is_nan(x) || is_inf(x) || x == 0.0) return x;
    const double a = abs(x);
    double r;
    if (a < 0.5) {
        const double z = a * a;          // x (1 + z/3! + z^2/5! + ...)
        const InvFactorials& f = inv_fact();
        double p = f.v[21];
        for (int k = 19; k >= 3; k -= 2) p = f.v[k] + z * p;
        r = a + a * z * p;
    } else if (a < 22.0) {
        const double e = exp(a);
        r = 0.5 * (e - 1.0 / e);
    } else if (a < 709.0) {
        r = 0.5 * exp(a);                 // e^-a is below half an ulp here
    } else {
        r = exp(a - constants::ln2);      // avoids overflow in exp(a) for a < 710.47
    }
    return x < 0.0 ? -r : r;
}

double cosh(double x) {
    if (is_nan(x)) return x;
    const double a = abs(x);
    if (is_inf(a)) return a;
    if (a < 22.0) {
        const double e = exp(a);
        return 0.5 * (e + 1.0 / e);
    }
    if (a < 709.0) return 0.5 * exp(a);
    return exp(a - constants::ln2);
}

double tanh(double x) {
    if (is_nan(x)) return x;
    const double a = abs(x);
    if (a > 22.0) return copysign(1.0, x);
    if (a < 1e-9) return x;
    double r;
    if (a < 0.5) {
        const double t = expm1(2.0 * a);
        r = t / (t + 2.0);
    } else {
        r = 1.0 - 2.0 / (exp(2.0 * a) + 1.0);
    }
    return x < 0.0 ? -r : r;
}

double csch(double x) { return 1.0 / sinh(x); }
double sech(double x) { return 1.0 / cosh(x); }
double coth(double x) { return 1.0 / tanh(x); }

double asinh(double x) {
    if (is_nan(x) || is_inf(x) || x == 0.0) return x;
    const double a = abs(x);
    double r;
    if (a > 1e8) {
        r = ln(a) + constants::ln2;
    } else {
        r = log1p(a + a * a / (1.0 + sqrt(1.0 + a * a)));
    }
    return x < 0.0 ? -r : r;
}

double acosh(double x) {
    if (is_nan(x)) return x;
    if (x < 1.0) return nan();
    if (is_inf(x)) return x;
    if (x > 1e8) return ln(x) + constants::ln2;
    const double t = x - 1.0;
    return log1p(t + sqrt(2.0 * t + t * t));
}

double atanh(double x) {
    if (is_nan(x)) return x;
    const double a = abs(x);
    if (a > 1.0) return nan();
    if (a == 1.0) return copysign(inf(), x);
    const double r = 0.5 * log1p(2.0 * a / (1.0 - a));
    return x < 0.0 ? -r : r;
}

double acsch(double x) {
    if (x == 0.0) return copysign(inf(), x);
    return asinh(1.0 / x);
}
double asech(double x) {
    if (is_nan(x)) return x;
    if (x <= 0.0 || x > 1.0) return nan();
    return acosh(1.0 / x);
}
double acoth(double x) {
    if (is_nan(x)) return x;
    if (abs(x) < 1.0) return nan();
    if (abs(x) == 1.0) return copysign(inf(), x);
    return atanh(1.0 / x);
}

// ---------------------------------------------------------------------------
// Formatting
// ---------------------------------------------------------------------------
std::string to_string(double x, int precision) {
    if (is_nan(x)) return "NaN";
    if (is_inf(x)) return x > 0 ? "Infinity" : "-Infinity";
    if (precision < 1) precision = 1;
    if (precision > 17) precision = 17;
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*g", precision, x);
    // Normalise exponent digits (MSVCRT prints e+021): keep at least one digit.
    char* e = std::strpbrk(buf, "eE");
    if (e) {
        char* d = e + 1;
        if (*d == '+' || *d == '-') ++d;
        char* z = d;
        while (*z == '0' && *(z + 1) != '\0') ++z;
        if (z != d) std::memmove(d, z, std::strlen(z) + 1);
    }
    return std::string(buf);
}

} // namespace cml
