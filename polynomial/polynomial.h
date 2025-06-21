#ifndef POLY_H
#define POLY_H

#include <iostream>
#include <cmath>
#include <cstddef>

#define POLY_EPSILON_DEFAULT 1e-38

class Poly {
private: 
    double* coeffs;
    int degree;
    bool immutable;

    void allocateCoeffs(int deg);
    void cleanup();
    void copyFrom(const Poly& other);

public:
    Poly();
    Poly(const double* coeffs, int deg);
    Poly(const Poly& other);
    ~Poly();

    Poly& operator=(const Poly& other);

    int getDegree() const;
    bool isImmutable() const;

    double operator[](int exp) const;
    double& operator[](int exp);
    void setCoefficient(int exp, double value);

    friend Poly operator+(const Poly& lhs, const Poly& rhs);
    friend Poly operator-(const Poly& lhs, const Poly& rhs);
    friend Poly operator*(const Poly& lhs, const Poly& rhs);
    friend Poly operator*(const Poly& p, double scalar);
    friend Poly operator*(double scalar, const Poly& p);
    friend Poly operator/(const Poly& dividend, const Poly& divisor);
    friend Poly operator%(const Poly& dividend, const Poly& divisor);

    Poly& operator+=(const Poly& rhs);
    Poly& operator-=(const Poly& rhs);
    Poly& operator*=(const Poly& rhs);
    Poly& operator*=(double scalar);

    friend bool operator==(const Poly& lhs, const Poly& rhs);
    friend bool operator!=(const Poly& lhs, const Poly& rhs);
    friend bool operator<(const Poly& lhs, const Poly& rhs);
    friend bool operator<=(const Poly& lhs, const Poly& rhs);
    friend bool operator>(const Poly& lhs, const Poly& rhs);
    friend bool operator>=(const Poly& lhs, const Poly& rhs);

    double evaluate(double x) const;
    Poly derivative() const;
    Poly integral() const;
    bool isZero() const;
    bool isMonic() const;
    Poly normalize() const;
    Poly compact(double epsilon = POLY_EPSILON_DEFAULT) const;

    friend Poly gcd(const Poly& p1, const Poly& p2);
    friend Poly* factor(const Poly& p, size_t& num_factors);
    friend double root(const Poly& p, double x0, double tolerance, int max_iterations, bool& success);

    friend std::ostream& operator<<(std::ostream& os, const Poly& p);
    friend std::istream& operator>>(std::istream& is, Poly& p);

    bool writeToFile(const char* filename) const;
    bool readFromFile(const char* filename);

    static Poly zero();

    // Legacy-style utility functions reinterpreted in C++:
    static Poly deflate(const Poly& P, double x);             // Returns deflated Q = (P - P(x)) / (x - r)
    static double evalDerivative(const Poly& P, double x);     // Value of f'(x)
    static void generateCSV(const Poly& P, double x1, double x2, int numPoints, const char* filename);
};

#endif // POLY_H
