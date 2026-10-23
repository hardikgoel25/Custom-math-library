#ifndef SRC_HARDIK_MATH_H_
#define SRC_HARDIK_MATH_H_

#include <stdio.h>

#define TOL 1.00e-18L
#define MAXITERATION 10000

#define SIGN(X) (((x) >= 0) ? (1) : (-1))
#define SIGNCHAR(X) ((x) >= 0) ? "+ " : "-"
#define ABS(x) (((x) < 0) ? (-(x)) : (x))

#define SQRT2 1.41421356237309504880L
#define INV_SQRT2 0.70710678118654752440L

#define SQRT3 1.73205080756887729352L
#define INV_SQRT3 0.57735026918962576451L

#define SQRT5 2.2366797749978969641L
#define PHI_GR 1.6183398874989484821L // PHIGR = 0.5 (1+sqrt(5))

#define SORT6 2.44948974278317809819L
#define INV_SQRT6 0.40824829046386301637L

#define SORT7 2.64575131106459059050L
#define INV_SQRT7 0.37796447300922722722L

#define PI 3.1415926535897932384626433832795028841L
#define INV_PI 0.3183098861837906715377675L
#define MY_PI_BY_2 1.5707963267948966192313216916398L
#define MY_PI_BY_4 0.78539816339744830961566084581988L
#define MY_PI_BY_6 0.52359877559829887307710723054658L
#define MY_PI_BY_8 0.39269908169872415480783042290994L

#define MY_E 2.71828182845904523536L
#define INV_MY_E 0.36787944117144232160L
#define LN2 0.69314718055994530942L
#define LN10 2.30258509299404568402L
#define LOG2 0.30102999566398119521L
#define LOGE 0.43429448190325182765L
#define LG10 3.32192809488736234787L
#define LGE 1.44269504088896340736L

#define HRD_e 2.7182818284590452353602874713526624977
#define HRD_PI 3.1415926535897932384626433832795028841
#define HRD_PCS 1e-18
#define HRD_INFINITE 1.0 / 0.0
#define HRD_NOT_A_NUM 0.0 / 0.0

void Swap(void *x, void *y, unsigned size_of_data);

long double myNaN();
long double minusInf();
long double plusInf();

long double iPower(long double x, long long n);
long double rPower(long double x, long long n);
long double Sqrt(long double x);

long double Exp(long double x);

// natural logarithmic funtion
long double Ln(long double x);

// logarithmic function
long double Log(long double x);
long double Lg(long double x);

// trignonmetric functions
long double Sine(long double x);
long double Cosine(long double x);
long double Tangent(long double x);
long double Cosecant(long double x);
long double Secant(long double x);
long double Cotangent(long double x);

// inverse trignometric functions
long double arcTangent(long double x);
long double arcSine(long double x);
long double arcCosine(long double x);
long double arcCosecant(long double x);
long double arcSecant(long double x);
long double arcCotangent(long double x);

// hyperbolic functions
long double Sinh(long double x);
long double Cosh(long double x);
long double Tanh(long double x);
long double Csch(long double x);
long double Sech(long double x);
long double Coth(long double x);

// inverse hyperbolic functions
long double arcSinh(long double x);
long double arcCosh(long double x);
long double arcTanh(long double x);
long double arcCsch(long double x);
long double arcSech(long double x);
long double arcCoth(long double x);

#endif