#include <iostream>
#include <cstdlib>
#include <math.h>
#include <iomanip>
#include "minimiser.h"
using namespace std;

void initFnData(FnData &data)
{
    data.x = 0;
    data.fx = 0;
    data.iterations = 0;
    data.addCount = 0;
    data.mulCount = 0;
}

void initPoint(Point &data)
{
    data.a = 0;
    data.b = 0;
    data.iterations = 0;
    data.addCount = 0;
    data.mulCount = 0;
}

int Fibonacci(int n)
{
    if (n <= 1)
        return n;
    return Fibonacci(n - 1) + Fibonacci(n - 2);
}

FnData LinMin(long double (*A)(long double x), long double a, long double b)
{
    struct FnData data;
    initFnData(data);
    long double fa, fb, x, y;
    int count = 0;
    x = a;
    y = x + TOL;
    data.addCount++;
    fa = A(x);
    fb = A(y);
    while (y < b)
    {
        data.iterations++;
        if (fb > fa) // If function starts increasing, minimum is at x
        {
            data.x = x;
            data.fx = fa;
            return data;
        }
        x = y;
        y += TOL;
        data.addCount++;
        fa = fb;
        fb = A(y);
        count++;
        if (abs(fa - fb) < TOL * TOL)
        {
            data.addCount++;
            data.x = x;
            data.fx = fa;
            return data;
        }
    }
    cout << "NOT CONVERGED" << endl;
    data.x = x;
    data.fx = A(x);
    return data;
}

FnData VarStepMin(long double (*f)(long double x), long double a, long double b)
{
    struct FnData data;
    initFnData(data);
    long double x, y, fa, fb, STEPSIZE, EPSILON, FACTOR;
    int count = 1;
    STEPSIZE = TOL;
    EPSILON = TOL;
    FACTOR = 2.0;
    x = a;
    fa = f(a);
    while (count < MAXITER)
    {
        y = x + STEPSIZE;
        data.addCount++;

        while (y > b || y < a)
        {
            STEPSIZE /= FACTOR;
            data.mulCount++;
            y = x + STEPSIZE;
            data.addCount++;

            if (abs(STEPSIZE) < TOL)
            {
                data.iterations = count;
                data.x = x;
                data.fx = f(x);
                return data;
            }
        }

        fb = f(y);

        if (fb < fa)
        {
            x = y;
            fa = fb;

            if (f(y + EPSILON) > fb)
            {
                EPSILON = -EPSILON;
                STEPSIZE = -STEPSIZE / FACTOR;
                data.addCount++;
                data.mulCount++;
            }
            else
            {
                STEPSIZE *= FACTOR;
                data.mulCount++;
            }
        }
        else
        {
            STEPSIZE /= FACTOR;
            data.mulCount++;
        }

        count++;

        if (abs(STEPSIZE) < TOL)
        {
            data.iterations = count;
            data.x = x;
            data.fx = fa;
            return data;
        }
    }

    data.iterations = count;
    data.x = x;
    data.fx = fa;
    cerr << "NOT CONVERGED\n";
    return data;
}
Point BracketMin(long double (*A)(long double x), long double a, long double b)
{
    struct Point data;
    initPoint(data);
    long double FACTOR, DELTA, x1, x2, x3, f1, f2, f3;
    int count = 1;
    FACTOR = 2.0;
    DELTA = 1.0e-2;
    x1 = a;
    f1 = A(x1);
    x2 = x1 + DELTA;
    data.addCount++;
    f2 = A(x2);
    while (1)
    {
        if (f2 > f1)
        {
            swap(x1, x2);
            swap(f1, f2);
            DELTA = -DELTA;
            data.addCount++;
        }
        DELTA = min(DELTA * FACTOR, b - x2);
        data.mulCount++;
        x3 = x2 + DELTA;
        data.addCount++;
        f3 = A(x3);
        if (f3 > f2)
        {
            break;
        }
        x1 = x2;
        f1 = f2;
        x2 = x3;
        f2 = f3;
        count++;
        if (count > 10000)
        { // Safety limit to prevent infinite loop
            std::cerr << "Warning: Maximum iteration limit reached in BracketMin.(function might be strictly increasing)\n";
            data.a = a;
            data.b = b;
            data.iterations = count;
            return (data);
        }
    }
    a = x1;
    b = x3;
    if (a > b)
    {
        swap(a, b);
    }
    data.a = a;
    data.b = b;
    data.iterations = count;
    return (data);
}
FnData GoldenMin(long double (*A)(long double x), long double a, long double b)
{
    struct FnData data;
    initFnData(data);
    long double d, alpha, x1, x2;
    int count = 0;
    d = b - a;
    data.addCount++;
    alpha = PHI_GR - 1;
    data.addCount++;
    while (b - a > TOL)
    {
        data.addCount++;
        d = d * alpha;
        data.mulCount++;
        x1 = b - d;
        data.addCount++;
        x2 = a + d;
        data.addCount++;
        count += 2;
        if (A(x1) < A(x2))
        {
            b = x2;
        }
        else
        {
            a = x1;
        }
    }
    data.x = (a + b) / 2.0;
    data.fx = A(data.x);
    data.iterations = count;
    return data;
}
FnData FibonacciMin(long double (*A)(long double x), long double a, long double b)
{
    struct FnData data;
    initFnData(data);
    long double d, x1, x2;
    int count = 0, N = 1, k;
    while (Fibonacci(N) < (b - a) / TOL)
    {
        data.addCount++;
        data.mulCount++;
        N++;
    }
    d = (b - a);
    data.addCount++;
    for (int k = 1; k < N; k++)
    {
        d = d * Fibonacci(N - k) / Fibonacci(N - k + 1);
        data.mulCount += 2;
        x1 = b - d;
        data.addCount++;
        x2 = a + d;
        data.addCount++;
        count += 2;
        if (A(x1) < A(x2))
        {
            b = x2;
        }
        else
        {
            a = x1;
        }
    }
    data.x = (a + b) / 2.0;
    data.fx = A(data.x);
    data.iterations = count;
    return data;
}
FnData NewtonMin(long double (*f1)(long double x), long double (*f2)(long double x), long double x)
{
    struct FnData data;
    initFnData(data);
    long double fx, fp, fpp, d;
    int count = 0;

    do
    {
        fp = f1(x);  // f'(x)
        fpp = f2(x); // f''(x)

        if (abs(fpp) < TOL)
        {
            cout << "SMALL SECOND DERIVATIVE" << endl;
            data.x = x;
            data.fx = fx;
            return data;
        }

        d = fp / fpp;
        data.mulCount++;
        x = x - d;
        data.addCount++;

        if (abs(d) < TOL)
        {
            cout << "CONVERGES" << endl;
            data.x = x;
            data.fx = fx;
            return data;
        }

        count++;
    } while (count < MAXITER);
    data.iterations = count;

    cout << "NOT CONVERGED" << endl;
    data.x = x;
    data.fx = fx;
    return data;
}

FnData BinaryMin(long double (*f)(long double x), long double a, long double b)
{
    struct FnData data;
    initFnData(data);
    long double c, fa = f(a), fb = f(b), fc, epsilon = TOL;
    int count = 2;
    while ((b - a) > epsilon)
    {
        data.addCount++;
        c = a + (b - a) / 2.0;
        data.addCount += 2;
        data.mulCount++;
        fc = f(c);
        if (f(c - epsilon) > fc)
        {
            data.addCount++;
            a = c;
            fa = fc;
        }
        else
        {
            data.addCount++;
            b = c;
            fb = fc;
        }
        count += 2;
    }
    data.x = (a + b) / 2.0;
    data.fx = f(data.x);
    data.iterations = count;
    return data;
}
