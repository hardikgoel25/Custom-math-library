/*NAME: HARDIK GOEL
ENROLLMENT NUMBER: 03416403223
COMPUTATIONAL METHODS
command used: g++ minimiserDriver.cpp minimiser.cpp -lm -o final*/
#ifndef MINIMAFINDER_H
#define MINIMAFINDER_H

#define TOL 1e-6
#define MAXITER 1000
#define sign(x) (((x) >= 0) ? (1) : (-1))
#define signchar(x) ((x) >= 0) ? "+ " : "-"
#define abs(x) (((x) < 0) ? (-(x)) : (x))
#define PHI_GR 1.6183398874989484821L

struct FnData
{
    long double x;
    long double fx;
    int iterations;
    int addCount;
    int mulCount;
};

struct Point
{
    long double a;
    long double b;
    int iterations;
    int addCount;
    int mulCount;
};

FnData LinMin(long double (*A)(long double x), long double a, long double b);
FnData VarStepMin(long double (*A)(long double x), long double a, long double b);
Point BracketMin(long double (*A)(long double x), long double a, long double b);
FnData GoldenMin(long double (*A)(long double x), long double a, long double b);
FnData FibonacciMin(long double (*A)(long double x), long double a, long double b);
FnData NewtonMin(long double (*f1)(long double x), long double (*f2)(long double x), long double a);
FnData BinaryMin(long double (*A)(long double x), long double a, long double b);

#endif