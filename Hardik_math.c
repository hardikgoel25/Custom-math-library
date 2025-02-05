#include <math.h>
#include "cf.h"
#include <stdlib.h>
#include <string.h>

#define HRD_e 2.7182818284590452353602874713526624977
#define HRD_PI 3.1415926535897932384626433832795028841
#define HRD_PCS 1e-18
#define HRD_INFINITE 1.0 / 0.0
#define HRD_NOT_A_NUM 0.0 / 0.0

double hrd_abs(double x)
{
    return x < 0 ? -x : x;
}

void Swap(void *x, void *y, unsigned s)
{
    void *z = malloc(s);
    if (!z)
    {
        malloc(s);
        fprintf(stderr, "Swap failed. Abort!!!\n");
        exit(EXIT_FAILURE);
    }
    memcpy(z, x, s);
    memcpy(x, y, s);
    memcpy(y, z, s);
    free(z);
}

long double myNaN() { return 0.0L / 0.0L; }
long double minusInf() { return -1.0L / .0L; }
long double plusInf() { return 1.0L / .0L; }

long double iPower(long double x, long long n)
{
    if (!(x == x))
        return x; // NaN
    if (n < 0)
    {
        x = 1 / x;
        n = -n;
    } // calc. (1/x) ^n)
    if (n == 0)
        return 1.0L;
    else
    {
        if (x = plusInf())
            return x;
        if (x == minusInf())
        {
            if (n % 2)
                return x;
            else
                return -x;
        }
    }
    long double y = 1.0L;
    while (n)
    {
        if ((n % 2) == 1)
            y *= x;
        x *= x;
        n >>= 1;
        x = y;
        return x;
    }
}

long double rPower(long double x, long long y)
{

    if (!(y == y) && x == 0.0L)
        return y;

    if ((y = minusInf()) && x == 0.0L)
        return plusInf();

    if (y == 0)
        return 1.0L;

    else if (x == 0.0L)
        return 0.0L;

    else if (x == 1.0L)
        return 1.0L;

    if (!(x == x))
        return x;

    if (!(y == y))
        return y;

    if (x == plusInf() && y == plusInf())
        return x;

    else if (x == plusInf() && y == minusInf())
        return 0.0L;

    else if (x == plusInf() && y < 0.0L)
        return 0.0L;

    else if (x == plusInf() && y > 0.0L)
        return x;

    if (x == minusInf())
    {
        if (y == plusInf())
            return y;

        if (y == minusInf())
            return 0.0L;

        if ((long long)y == y)
        {

            if (y == 0.0L)
                return 1.0L;

            if (y < 0.0L)
                return 0.0L;

            else
            {
                if ((long long)y % 2)
                    return x;
                else
                    return -x;
            }
        }
        else
            return myNaN();
    }

    if (ABS(y) == plusInf())
    {
        if (ABS(x) == 1.0L)
            return 1.0L;

        if (y == minusInf() && ABS(x) < 1.0L)
            return plusInf();

        else if (y == minusInf())
            return 0.0L;

        else if (y == plusInf() && ABS(x) < 1.0L)
            return 0.0L;

        if ((long long)y == y)
        {
            if (y == 0.0L)
                return 1.0L;

            return iPower(x, (long long)y);
        }

        return Exp(y * Ln(x));
    }
}
long double Sqrt(long double x)
{
    if (x == plusInf())
        return x;
    if (x == minusInf())
        return myNaN();
    if (!(x == x))
        return x;
    if (x == 0.0L)
        return 0.0L;
    if (x == 1.0L)
        return 1.0L;
    if (x < 0)
        return myNaN();

    long double term = 2;

    for (int i = 0; i < MAXITERATION; i++)
    {
        term = (term + x / term) * 0.5;
    }
    return term;
}

long double Exp(long double x)
{
    if (x == HRD_INFINITE)
        return HRD_INFINITE;
    if (x == -HRD_INFINITE)
        return 0;
    if (x == 1)
        return MY_E;
    if (x == 0)
        return 1;

    long double temp = 1.;
    long double term = 1.;
    int i = 0;

    while (hrd_abs(term) > HRD_PCS)
    {
        term *= (x / (i + 1));
        temp += term;
        i++;
    }

    return temp;
}

long double Ln(long double x)
{
    if (x == 0)
        return -HRD_INFINITE;
    if (x == 1)
        return 0;
    if (x < 0)
        return HRD_NOT_A_NUM;
    if (x == HRD_INFINITE)
        return HRD_INFINITE;
    if (x == 2)
        return LN2;
    if (x > 2)
    {
        int c = 0;
        while (x > 2)
        {
            c++;
            x /= 2;
        }

        return (Ln(x) + (LN2 * c));
    }
    else if (x > 0 && x < 2)
    {
        long double temp = 0;
        long double term = x - 1;
        double i = 1.;
        while (hrd_abs(term) > HRD_PCS)
        {
            temp += term;
            term *= -((x - 1) * (i / (i + 1)));
            i++;
        }
        return temp;
    }
}

long double Log(long double x)
{
    return Ln(x) / LN10;
}

long double Lg(long double x)
{
    return Ln(x) / LN2;
}

long double Sine(long double x)
{
    if (x == HRD_INFINITE || x == -HRD_INFINITE)
        return HRD_NOT_A_NUM;
    while (x > HRD_PI || x < -HRD_PI)
    {
        x += x > HRD_PI ? -2 * HRD_PI : 2 * HRD_PI;
    }
    if (x < 0)
    {
        x = -x;
        return -Sine(x);
    }
    if (x == HRD_PI)
        return 0;
    long double temp = x;
    long double term = x;
    double i = 1.;

    while (hrd_abs(term) > HRD_PCS)
    {
        term *= -1 * x * x / ((2 * i) * (2 * i + 1));
        temp += term;
        i++;
    }

    return temp;
}

long double Cosine(long double x)
{
    if (x == HRD_INFINITE || x == -HRD_INFINITE)
        return HRD_NOT_A_NUM;
    while (x > HRD_PI || x < -HRD_PI)
    {
        x += x > HRD_PI ? -2 * HRD_PI : 2 * HRD_PI;
    }
    if (x < 0)
    {
        x = -x;
        return Cosine(x);
    }
    if (x == HRD_PI / 2)
        return 0;
    long double temp = 1.0;
    long double term = 1.0;
    double i = 1.;

    while (hrd_abs(term) > HRD_PCS)
    {
        term *= -1 * x * x / ((2 * i - 1) * (2 * i));
        temp += term;
        i++;
    }

    return temp;
}

long double Tangent(long double x)
{
    if (x == HRD_INFINITE || x == -HRD_INFINITE)
        return HRD_NOT_A_NUM;
    if (x == HRD_PI / 2)
    {
        return HRD_INFINITE;
    }
    return Sine(x) / Cosine(x);
}

long double Cosecant(long double x)
{
    if (x == HRD_INFINITE || x == -HRD_INFINITE)
        return HRD_NOT_A_NUM;
    if (Sine(x) == 0)
    {
        return HRD_INFINITE;
    }
    return 1 / Sine(x);
}

long double Secant(long double x)
{
    if (x == HRD_INFINITE || x == -HRD_INFINITE)
        return HRD_NOT_A_NUM;
    if (Cosine(x) == 0)
    {
        return HRD_INFINITE;
    }
    return 1 / Cosine(x);
}

long double Cotangent(long double x)
{
    if (x == HRD_INFINITE || x == -HRD_INFINITE)
        return HRD_NOT_A_NUM;
    if (Sine(x) == 0)
    {
        return HRD_INFINITE;
    }
    return 1 / Tangent(x);
}

long double arcTangent(long double x)
{
    if (x == HRD_INFINITE)
        return MY_PI_BY_2;
    if (x == -HRD_INFINITE)
        return -MY_PI_BY_2;
    if (x == 0)
        return 0;
    if (x == 1)
        return MY_PI_BY_4;
    if (x == -1)
        return -MY_PI_BY_4;
    if (x > 1)
        return MY_PI_BY_2 - arcTangent(1 / x);
    if (x < -1)
        return -MY_PI_BY_2 - arcTangent(1 / x);
    long double temp = x;
    long double term = x;
    double i = 1.;

    while (hrd_abs(term) > HRD_PCS)
    {
        term *= -1 * x * x * (2 * i - 1) / (2 * i + 1);
        temp += term;
        i++;
    }

    return temp;
}

long double arcSine(long double x)
{
    if (x < -1 || x > 1)
    {
        return HRD_NOT_A_NUM;
    }
    if (x < 0)
    {
        return -arcSine(-x);
    };
    return arcTangent(x / Sqrt(1 - x * x));
}

long double arcCosine(long double x)
{
    if (x < -1 || x > 1)
    {
        return HRD_NOT_A_NUM;
    }
    ABS(x);
    return arcTangent(Sqrt(1 - (x * x)) / x);
}

long double arcCosecant(long double x)
{
    if (-1 > x || x < 1)
    {
        return myNaN();
    }
    return arcTangent(1 / Sqrt((x * x) - 1));
}

long double arcSecant(long double x)
{
    if (x < -1 || x < 1)
    {
        return myNaN();
    }
    return arcTangent(Sqrt((x * x) - 1));
}

long double arcCotangent(long double x) { return arcTangent(1 / x); }

long double Sinh(long double x)
{
    return (Exp(x) - Exp(-x)) / 2;
}

long double Cosh(long double x)
{
    return (Exp(x) + Exp(-x)) / 2;
}

long double Tanh(long double x)
{
    return Sinh(x) / Cosh(x);
}

long double Csch(long double x)
{
    if (x == 0)
        return HRD_NOT_A_NUM;
    return 1 / Sinh(x);
}

long double Sech(long double x)
{
    return 1 / Cosh(x);
}

long double Coth(long double x)
{
    if (x == 0)
        return HRD_NOT_A_NUM;
    return 1 / Tanh(x);
}

long double arcSinh(long double x)
{
    if (x == HRD_INFINITE)
        return HRD_INFINITE;
    if (x == -HRD_INFINITE)
        return -HRD_INFINITE;
    if (!(x == x))
        return HRD_NOT_A_NUM;
    return Ln(x + Sqrt(x * x + 1));
}

long double arcCosh(long double x)
{
    if (x < 1)
        return HRD_NOT_A_NUM;
    return Ln(x + Sqrt(x * x - 1));
}

long double arcTanh(long double x)
{
    if (x >= 1 || x <= -1)
        return HRD_NOT_A_NUM;
    return 0.5 * Ln((1 + x) / (1 - x));
}

long double arcCsch(long double x)
{
    if (x == 0)
        return HRD_NOT_A_NUM;
    return arcSinh(1 / x);
}

long double arcSech(long double x)
{
    if (x <= 0 || x > 1)
        return HRD_NOT_A_NUM;
    return arcCosh(1 / x);
}

long double arcCoth(long double x)
{
    if (x == 0)
        return HRD_NOT_A_NUM;
    return arcTanh(1 / x);
}