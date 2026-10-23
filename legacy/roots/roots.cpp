#include <iostream>
#include <cstdlib>
#include <math.h>
#include <iomanip>
#include "roots.h"
#include "polynomial.h"

using namespace std;

#define TOL 1e-18
#define MAXITER 1000
#define sign(x) (((x) >= 0) ? (1) : (-1))
#define signchar(x) ((x) >= 0) ? "+ " : "-"
#define abs(x) (((x) < 0) ? (-(x)) : (x))

void initFnData(FnData &data)
{
    data.x = 0;
    data.fx = 0;
    data.iterations = 0;
    data.addCount = 0;
    data.mulCount = 0;
}

FnData LinSearchRootFn(long double (*f)(long double x), long double a, long double b)
{
    const double tol = 1e-8;
    struct FnData data;
    initFnData(data);
    long double x, y, fx, fy;
    x = a;
    fx = f(x);
    while (x <= b)
    {
        data.iterations++;
        y = x + tol;
        data.addCount++;
        fy = f(y);
        if (abs(fy) < tol * 10) // Check if function is close to zero (for cases like x^2 which are always +ve)
        {
            break;
        }
        if (sign(fx) != sign(fy))
        {
            break;
        }
        x = y;
        fx = fy;
    }
    x = (x + y) / 2;
    data.mulCount++;
    data.x = x;
    data.fx = f(x);
    return data;
}
FnData VarStepRootFn(long double (*f)(long double x), long double a, long double b)
{
    struct FnData data;
    initFnData(data);
    long double x, y, fx, fy, d, FACTOR;
    x = a;
    fx = f(x);
    d = TOL;
    FACTOR = 1.1;
    int oscillation_count = 0;
    long double prev_x = x, prev_fx = fx;

    while (x <= b)
    {
        data.iterations++;
        y = x + d;
        data.addCount++;
        fy = f(y);

        if (abs(fx) < TOL)
        {
            break;
        }

        if (abs(y - prev_x) < TOL && abs(fy - prev_fx) < TOL)
            oscillation_count++;
        else
            oscillation_count = 0;

        if (oscillation_count > 5)
            break;
        if (sign(fx) != sign(fy))
        {
            d = -sign(d) * TOL;
            data.mulCount++;
        }
        else
        {
            d = FACTOR * d;
            data.mulCount++;
        }

        prev_x = x;
        prev_fx = fx;
        x = y;
        fx = fy;
    }

    data.x = x;
    data.fx = f(x);
    return data;
}

FnData BisectionRootFn(long double (*f)(long double), long double a, long double b)
{
    struct FnData data;
    initFnData(data);

    long double fa = f(a), fb = f(b), c, fc;
    if (abs(fa) < TOL)
    {
        data.x = a;
        data.fx = fa;
        return data;
    }
    if (abs(fb) < TOL)
    {
        data.x = b;
        data.fx = fb;
        return data;
    }
    for (int n = 0; n < MAXITER; n++)
    {
        data.iterations++;
        c = (a + b) / 2.0;
        data.addCount++;
        data.mulCount++;
        fc = f(c);

        if (abs(fc) < TOL || (b - a) < TOL)
        {
            data.x = c;
            data.fx = fc;
            return data;
        }

        if (fa * fc < 0)
        {
            b = c;
            fb = fc;
        }
        else
        {
            a = c;
            fa = fc;
        }
        data.mulCount++;
    }
    data.x = c;
    data.fx = fc;
    return data;
}

FnData RegulaFalsiRootFn(long double (*f)(long double x), long double a, long double b)
{
    struct FnData data;
    initFnData(data);
    long double fa = f(a), fb = f(b), c, fc;
    int n = 1;

    if (abs(fa) < TOL)
    {
        data.x = a;
        data.fx = fa;
        return data;
    }
    if (abs(fb) < TOL)
    {
        data.x = b;
        data.fx = fb;
        return data;
    }
    do
    {
        data.iterations++;
        long double denominator = fb - fa;
        data.addCount++;
        if (abs(denominator) < TOL)
        {
            cout << "Small denominator, returning closer value" << endl;
            data.x = (abs(fa) < abs(fb)) ? a : b;
            data.fx = (abs(fa) < abs(fb)) ? fa : fb;
            return data;
        }

        c = a - fa * (b - a) / denominator;
        data.addCount += 2;
        data.mulCount += 2;
        fc = f(c);

        if (abs(fc) < TOL)
        {
            data.x = c;
            data.fx = fc;
            return data;
        }
        n++;

        if (sign(fc) == sign(fa))
        {
            a = c;
            fa = fc;
        }
        else
        {
            b = c;
            fb = fc;
        }
    } while (n < MAXITER);
    data.x = c;
    data.fx = fc;
    return data;
}
FnData NewtonRootFn(long double (*f)(long double x), long double (*f1)(long double x), long double x)
{
    struct FnData data;
    initFnData(data);
    long double fx, fp, d;
    int count = 0;
    fx = f(x);
    do
    {
        data.iterations++;
        fp = f1(x);
        if (abs(fp) < TOL)
        {
            cout << right << setw(25) << "SMALL DERIVATIVE" << endl;
            data.x = x;
            data.fx = fx;
            return data;
        }
        d = fx / fp;
        data.mulCount++;
        x = x - d;
        data.addCount++;
        fx = f(x);
        if (abs(d) < TOL)
        {
            cout << right << setw(25) << "(CONVERGES)" << endl;
            data.x = x;
            data.fx = fx;
            return data;
        }
        count++;
    } while (count < MAXITER);
    cout << right << setw(25) << "(NOT CONVERGES)" << endl;
    data.x = x;
    data.fx = fx;
    return data;
}
FnData SecantRootFn(long double (*f)(long double), long double a, long double b)
{
    struct FnData data;
    initFnData(data);
    long double fa = f(a), fb = f(b);

    for (int i = 0; i < MAXITER; i++)
    {
        data.iterations++;
        if (abs(fb - fa) < TOL)
        {
            cerr << "Warning: Small denominator, possible stagnation.\n";
            data.x = (abs(fa) < abs(fb)) ? a : b;
            data.fx = (abs(fa) < abs(fb)) ? fa : fb;
            return data;
        }

        long double c = b - fb * (b - a) / (fb - fa);
        data.addCount += 3;
        data.mulCount += 2;

        if (c < a || c > b)
        {
            c = (a + b) / 2.0;
            data.addCount++;
            data.mulCount++;
        }

        long double fc = f(c);

        if (abs(fc) < TOL || abs(b - a) < TOL)
        {
            data.x = c;
            data.fx = fc;
            return data;
        }

        if (fa * fc < 0)
        {
            b = c;
            fb = fc;
        }
        else
        {
            a = c;
            fa = fc;
        }
        data.mulCount++;
    }
    cout << right << setw(25) << "(NOT CONVERGED)\n";
    data.x = (a + b) / 2.0;
    data.mulCount++;
    data.fx = f(data.x);
    return data;
}
FnData LinSearchRootPoly(const Poly &P, long double a, long double b)
{
    const double tol = 1e-8;
    FnData data;
    initFnData(data);
    long double x = a, y, fx = P.evaluate(x), fy;

    while (x <= b)
    {
        data.iterations++;
        y = x + tol;
        data.addCount++;
        fy = P.evaluate(y);

        if (abs(fy) < tol * 10)
            break;
        if (signbit(fx) != signbit(fy))
            break;

        x = y;
        fx = fy;
    }

    x = (x + y) / 2;
    data.mulCount++;
    data.x = x;
    data.fx = P.evaluate(x);
    return data;
}

FnData VarStepRootPoly(const Poly &P, long double a, long double b)
{
    FnData data;
    initFnData(data);
    long double x = a, fx = P.evaluate(x), d = TOL, FACTOR = 1.1;
    int oscillation_count = 0;
    long double prev_x = x, prev_fx = fx, y, fy;

    while (x <= b)
    {
        data.iterations++;
        y = x + d;
        data.addCount++;
        fy = P.evaluate(y);

        if (abs(fx) < TOL)
            break;
        if (abs(y - prev_x) < TOL && abs(fy - prev_fx) < TOL)
            oscillation_count++;
        else
            oscillation_count = 0;

        if (oscillation_count > 5)
            break;

        if (signbit(fx) != signbit(fy))
        {
            d = -copysign(TOL, d);
            data.mulCount++;
        }
        else
        {
            d = FACTOR * d;
            data.mulCount++;
        }

        prev_x = x;
        prev_fx = fx;
        x = y;
        fx = fy;
    }

    data.x = x;
    data.fx = P.evaluate(x);
    return data;
}

FnData BisectionRootPoly(const Poly &P, long double a, long double b)
{
    FnData data;
    initFnData(data);

    long double fa = P.evaluate(a), fb = P.evaluate(b), c = 0, fc = 0;
    if (abs(fa) < TOL)
        return {a, fa, 0, 0, 0};
    if (abs(fb) < TOL)
        return {b, fb, 0, 0, 0};

    for (int n = 0; n < MAXITER; n++)
    {
        data.iterations++;
        c = (a + b) / 2.0;
        data.addCount++;
        data.mulCount++;
        fc = P.evaluate(c);

        if (abs(fc) < TOL || (b - a) < TOL)
            break;

        if (fa * fc < 0)
        {
            b = c;
            fb = fc;
        }
        else
        {
            a = c;
            fa = fc;
        }
        data.mulCount++;
    }

    data.x = c;
    data.fx = fc;
    return data;
}

FnData RegulaFalsiRootPoly(const Poly &P, long double a, long double b)
{
    FnData data;
    initFnData(data);

    long double fa = P.evaluate(a), fb = P.evaluate(b), c = 0, fc = 0;
    int n = 1;

    if (abs(fa) < TOL)
        return {a, fa, 0, 0, 0};
    if (abs(fb) < TOL)
        return {b, fb, 0, 0, 0};

    do
    {
        data.iterations++;
        long double denominator = fb - fa;
        data.addCount++;

        if (abs(denominator) < TOL)
        {
            data.x = (abs(fa) < abs(fb)) ? a : b;
            data.fx = (abs(fa) < abs(fb)) ? fa : fb;
            return data;
        }

        c = a - fa * (b - a) / denominator;
        data.addCount += 2;
        data.mulCount += 2;
        fc = P.evaluate(c);

        if (abs(fc) < TOL)
            break;

        if (signbit(fc) == signbit(fa))
        {
            a = c;
            fa = fc;
        }
        else
        {
            b = c;
            fb = fc;
        }
        n++;
    } while (n < MAXITER);

    data.x = c;
    data.fx = fc;
    return data;
}

FnData NewtonRootPoly(const Poly &P, long double x)
{
    FnData data;
    initFnData(data);

    long double fx = P.evaluate(x), fp, d;
    int count = 0;

    do
    {
        data.iterations++;
        Poly P1 = P.derivative();
        fp = P1.evaluate(x);

        if (abs(fp) < TOL)
        {
            data.x = x;
            data.fx = fx;
            return data;
        }

        d = fx / fp;
        data.mulCount++;
        x = x - d;
        data.addCount++;
        fx = P.evaluate(x);

        if (abs(d) < TOL)
            break;

        count++;
    } while (count < MAXITER);

    data.x = x;
    data.fx = fx;
    return data;
}

FnData SecantRootPoly(const Poly &P, long double a, long double b)
{
    FnData data;
    initFnData(data);
    long double fa = P.evaluate(a), fb = P.evaluate(b);

    if (abs(fa) < TOL)
        return {a, fa, 0, 0, 0};
    if (abs(fb) < TOL)
        return {b, fb, 0, 0, 0};

    for (int i = 0; i < MAXITER; i++)
    {
        data.iterations++;

        if (abs(fb - fa) < TOL)
        {
            data.x = (abs(fa) < abs(fb)) ? a : b;
            data.fx = (abs(fa) < abs(fb)) ? fa : fb;
            return data;
        }

        long double c = b - fb * (b - a) / (fb - fa);
        data.addCount += 3;
        data.mulCount += 2;

        if (c < a || c > b)
        {
            c = (a + b) / 2.0;
            data.addCount++;
            data.mulCount++;
        }

        long double fc = P.evaluate(c);

        if (abs(fc) < TOL || abs(b - a) < TOL)
        {
            data.x = c;
            data.fx = fc;
            return data;
        }

        if (fa * fc < 0)
        {
            b = c;
            fb = fc;
        }
        else
        {
            a = c;
            fa = fc;
        }
        data.mulCount++;
    }

    data.x = (a + b) / 2.0;
    data.mulCount++;
    data.fx = P.evaluate(data.x);
    return data;
}
