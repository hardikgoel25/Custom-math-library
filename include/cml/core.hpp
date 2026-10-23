// cml/core.hpp — elementary functions implemented from scratch (no <cmath>).
//
// Every routine here is written against IEEE-754 double semantics: NaN and
// infinities propagate the way the C standard library propagates them, and
// the implementations use proper range reduction plus convergence checks
// instead of fixed iteration counts.  Accuracy is typically within a few ULP
// of the platform <cmath> over the tested ranges (see tests/test_core.cpp).
#ifndef CML_CORE_HPP
#define CML_CORE_HPP

#include <cstdint>
#include <string>

namespace cml {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
namespace constants {
constexpr double pi         = 3.141592653589793238462643383279502884;
constexpr double tau        = 6.283185307179586476925286766559005768;
constexpr double half_pi    = 1.570796326794896619231321691639751442;
constexpr double quarter_pi = 0.785398163397448309615660845819875721;
constexpr double inv_pi     = 0.318309886183790671537767526745028724;
constexpr double e          = 2.718281828459045235360287471352662498;
constexpr double inv_e      = 0.367879441171442321595523770161460867;
constexpr double ln2        = 0.693147180559945309417232121458176568;
constexpr double ln10       = 2.302585092994045684017991454684364208;
constexpr double log2e      = 1.442695040888963407359924681001892137;
constexpr double log10e     = 0.434294481903251827651128918916605082;
constexpr double sqrt2      = 1.414213562373095048801688724209698079;
constexpr double inv_sqrt2  = 0.707106781186547524400844362104849039;
constexpr double sqrt3      = 1.732050807568877293527446341505872367;
constexpr double inv_sqrt3  = 0.577350269189625764509148780501957456;
constexpr double sqrt5      = 2.236067977499789696409173668731276235;
constexpr double phi        = 1.618033988749894848204586834365638118; // golden ratio
constexpr double euler_gamma= 0.577215664901532860606512090082402431;
constexpr double deg_per_rad= 57.29577951308232087679815481410517033;
constexpr double rad_per_deg= 0.017453292519943295769236907684886127;
constexpr double epsilon    = 2.220446049250313080847263336181640625e-16; // DBL_EPSILON
constexpr double max_value  = 1.7976931348623157e308;
constexpr double min_normal = 2.2250738585072014e-308;
} // namespace constants

// ---------------------------------------------------------------------------
// Classification / special values
// ---------------------------------------------------------------------------
double nan();
double inf();
bool   is_nan(double x);
bool   is_inf(double x);
bool   is_finite(double x);
bool   is_integer(double x);   // finite and has no fractional part
int    sign(double x);         // -1, 0, +1 (0 for NaN too)
bool   sign_bit(double x);     // true if the sign bit is set (also for -0.0)

// ---------------------------------------------------------------------------
// Basic arithmetic helpers
// ---------------------------------------------------------------------------
double abs(double x);
double fmin(double a, double b);
double fmax(double a, double b);
double clamp(double x, double lo, double hi);
double floor(double x);
double ceil(double x);
double trunc(double x);
double round(double x);        // half away from zero
double fmod(double x, double y);
double copysign(double mag, double sgn);
double ldexp(double x, int exp);          // x * 2^exp
double frexp(double x, int* exp);         // x = m * 2^exp, 0.5 <= |m| < 1
bool   nearly_equal(double a, double b, double rel = 1e-12, double abs_tol = 1e-15);
double deg2rad(double deg);
double rad2deg(double rad);

// ---------------------------------------------------------------------------
// Powers and roots
// ---------------------------------------------------------------------------
double ipow(double x, long long n);  // exponentiation by squaring
double pow(double x, double y);      // full IEEE special-case handling
double sqrt(double x);
double cbrt(double x);
double hypot(double x, double y);
double nth_root(double x, int n);

// ---------------------------------------------------------------------------
// Exponentials and logarithms
// ---------------------------------------------------------------------------
double exp(double x);
double exp2(double x);
double expm1(double x);
double ln(double x);
double log(double x);          // alias of ln
double log2(double x);
double log10(double x);
double log_base(double base, double x);
double log1p(double x);

// ---------------------------------------------------------------------------
// Trigonometry (radians)
// ---------------------------------------------------------------------------
double sin(double x);
double cos(double x);
double tan(double x);
double csc(double x);
double sec(double x);
double cot(double x);
void   sincos(double x, double* s, double* c);

double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);
double acsc(double x);
double asec(double x);
double acot(double x);

// ---------------------------------------------------------------------------
// Hyperbolic
// ---------------------------------------------------------------------------
double sinh(double x);
double cosh(double x);
double tanh(double x);
double csch(double x);
double sech(double x);
double coth(double x);

double asinh(double x);
double acosh(double x);
double atanh(double x);
double acsch(double x);
double asech(double x);
double acoth(double x);

// ---------------------------------------------------------------------------
// Formatting helpers (used by the engine and CLI)
// ---------------------------------------------------------------------------
std::string to_string(double x, int precision = 15);

} // namespace cml

#endif // CML_CORE_HPP
