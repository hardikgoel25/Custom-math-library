#ifndef ROOTFINDER_H
#define ROOTFINDER_H
#include "polynomial.h"

// Structure to hold the result of the root-finding process
struct FnData
{
    long double x;        // Root found
    long double fx;       // Function value at the root
    int iterations;       // Number of iterations performed
    int addCount;         // Number of additions performed
    int mulCount;         // Number of multiplications performed
};

// Root-finding methods for general functions (function pointers)
FnData LinSearchRootFn(long double (*f)(long double x), long double a, long double b);
FnData VarStepRootFn(long double (*f)(long double x), long double a, long double b);
FnData BisectionRootFn(long double (*f)(long double x), long double a, long double b);
FnData RegulaFalsiRootFn(long double (*f)(long double x), long double a, long double b);
FnData NewtonRootFn(long double (*f)(long double x), long double (*f1)(long double x), long double x);
FnData SecantRootFn(long double (*f)(long double x), long double a, long double b);

// Root-finding methods for polynomial objects
FnData LinSearchRootPoly(const Poly& P, long double a, long double b);   // Polynomial version
FnData VarStepRootPoly(const Poly& P, long double a, long double b);     // Polynomial version
FnData BisectionRootPoly(const Poly& P, long double a, long double b);   // Polynomial version
FnData RegulaFalsiRootPoly(const Poly& P, long double a, long double b); // Polynomial version
FnData NewtonRootPoly(const Poly& P, long double x);                      // Polynomial version
FnData SecantRootPoly(const Poly& P, long double a, long double b);       // Polynomial version

#endif // ROOTFINDER_H
