#include "polynomial.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <limits>

Poly::Poly() : coeffs(nullptr), degree(-1), immutable(false) {}

Poly::Poly(const double* c, int deg) : degree(deg), immutable(false) {
    allocateCoeffs(degree);
    std::copy(c, c + degree + 1, coeffs);
}

Poly::Poly(const Poly& other) {
    copyFrom(other);
}

Poly::~Poly() {
    cleanup();
}

Poly& Poly::operator=(const Poly& other) {
    if (this != &other) {
        cleanup();
        copyFrom(other);
    }
    return *this;
}

void Poly::allocateCoeffs(int deg) {
    coeffs = new double[deg + 1]();
}

void Poly::cleanup() {
    delete[] coeffs;
    coeffs = nullptr;
    degree = -1;
}

void Poly::copyFrom(const Poly& other) {
    degree = other.degree;
    immutable = other.immutable;
    allocateCoeffs(degree);
    std::copy(other.coeffs, other.coeffs + degree + 1, coeffs);
}

int Poly::getDegree() const {
    return degree;
}

bool Poly::isImmutable() const {
    return immutable;
}

double Poly::operator[](int exp) const {
    if (exp < 0 || exp > degree) return 0.0;
    return coeffs[exp];
}

double& Poly::operator[](int exp) {
    if (exp > degree) {
        double* newCoeffs = new double[exp + 1]();
        std::copy(coeffs, coeffs + degree + 1, newCoeffs);
        delete[] coeffs;
        coeffs = newCoeffs;
        degree = exp;
    }
    return coeffs[exp];
}

void Poly::setCoefficient(int exp, double value) {
    (*this)[exp] = value;
}

Poly operator+(const Poly& lhs, const Poly& rhs) {
    int maxDeg = std::max(lhs.degree, rhs.degree);
    double* resultCoeffs = new double[maxDeg + 1]();
    for (int i = 0; i <= maxDeg; i++) {
        if (i <= lhs.degree) resultCoeffs[i] += lhs[i];
        if (i <= rhs.degree) resultCoeffs[i] += rhs[i];
    }
    Poly result(resultCoeffs, maxDeg);
    delete[] resultCoeffs;
    return result;
}

Poly operator-(const Poly& lhs, const Poly& rhs) {
    int maxDeg = std::max(lhs.degree, rhs.degree);
    double* resultCoeffs = new double[maxDeg + 1]();
    for (int i = 0; i <= maxDeg; i++) {
        if (i <= lhs.degree) resultCoeffs[i] += lhs[i];
        if (i <= rhs.degree) resultCoeffs[i] -= rhs[i];
    }
    Poly result(resultCoeffs, maxDeg);
    delete[] resultCoeffs;
    return result;
}

Poly operator*(const Poly& lhs, const Poly& rhs) {
    int deg = lhs.degree + rhs.degree;
    double* resultCoeffs = new double[deg + 1]();
    for (int i = 0; i <= lhs.degree; i++)
        for (int j = 0; j <= rhs.degree; j++)
            resultCoeffs[i + j] += lhs[i] * rhs[j];
    Poly result(resultCoeffs, deg);
    delete[] resultCoeffs;
    return result;
}

Poly operator*(const Poly& p, double scalar) {
    double* resultCoeffs = new double[p.degree + 1];
    for (int i = 0; i <= p.degree; i++)
        resultCoeffs[i] = p[i] * scalar;
    Poly result(resultCoeffs, p.degree);
    delete[] resultCoeffs;
    return result;
}

Poly operator*(double scalar, const Poly& p) {
    return p * scalar;
}

Poly operator/(const Poly& dividend, const Poly& divisor) {
    if (divisor.isZero()) throw std::runtime_error("Division by zero polynomial");
    if (dividend.degree < divisor.degree) return Poly::zero();

    Poly quotient;
    Poly remainder = dividend;

    quotient.allocateCoeffs(dividend.degree - divisor.degree);
    quotient.degree = dividend.degree - divisor.degree;

    for (int i = dividend.degree; i >= divisor.degree; i--) {
        double factor = remainder[i] / divisor[divisor.degree];
        quotient[i - divisor.degree] = factor;
        for (int j = 0; j <= divisor.degree; j++) {
            remainder[i - j] -= factor * divisor[divisor.degree - j];
        }
    }
    return quotient;
}

Poly operator%(const Poly& dividend, const Poly& divisor) {
    return dividend - (dividend / divisor) * divisor;
}

Poly& Poly::operator+=(const Poly& rhs) {
    *this = *this + rhs;
    return *this;
}

Poly& Poly::operator-=(const Poly& rhs) {
    *this = *this - rhs;
    return *this;
}

Poly& Poly::operator*=(const Poly& rhs) {
    *this = *this * rhs;
    return *this;
}

Poly& Poly::operator*=(double scalar) {
    *this = *this * scalar;
    return *this;
}

bool operator==(const Poly& lhs, const Poly& rhs) {
    if (lhs.degree != rhs.degree) return false;
    for (int i = 0; i <= lhs.degree; i++)
        if (std::abs(lhs[i] - rhs[i]) > POLY_EPSILON_DEFAULT)
            return false;
    return true;
}

bool operator!=(const Poly& lhs, const Poly& rhs) {
    return !(lhs == rhs);
}

bool operator<(const Poly& lhs, const Poly& rhs) {
    return lhs.degree < rhs.degree;
}

bool operator<=(const Poly& lhs, const Poly& rhs) {
    return lhs.degree <= rhs.degree;
}

bool operator>(const Poly& lhs, const Poly& rhs) {
    return lhs.degree > rhs.degree;
}

bool operator>=(const Poly& lhs, const Poly& rhs) {
    return lhs.degree >= rhs.degree;
}

double Poly::evaluate(double x) const {
    double result = 0;
    for (int i = degree; i >= 0; --i)
        result = result * x + coeffs[i];
    return result;
}

Poly Poly::derivative() const {
    if (degree <= 0) return Poly::zero();
    double* derivCoeffs = new double[degree];
    for (int i = 1; i <= degree; i++)
        derivCoeffs[i - 1] = coeffs[i] * i;
    Poly result(derivCoeffs, degree - 1);
    delete[] derivCoeffs;
    return result;
}

Poly Poly::integral() const {
    double* intCoeffs = new double[degree + 2]();
    for (int i = 0; i <= degree; i++)
        intCoeffs[i + 1] = coeffs[i] / (i + 1);
    Poly result(intCoeffs, degree + 1);
    delete[] intCoeffs;
    return result;
}

bool Poly::isZero() const {
    for (int i = 0; i <= degree; i++)
        if (std::abs(coeffs[i]) > POLY_EPSILON_DEFAULT)
            return false;
    return true;
}

bool Poly::isMonic() const {
    return std::abs(coeffs[degree] - 1.0) < POLY_EPSILON_DEFAULT;
}

Poly Poly::normalize() const {
    if (isZero()) return *this;
    double lead = coeffs[degree];
    return *this * (1.0 / lead);
}

Poly Poly::compact(double epsilon) const {
    int newDeg = degree;
    while (newDeg >= 0 && std::abs(coeffs[newDeg]) < epsilon)
        newDeg--;
    double* newCoeffs = new double[newDeg + 1];
    std::copy(coeffs, coeffs + newDeg + 1, newCoeffs);
    Poly result(newCoeffs, newDeg);
    delete[] newCoeffs;
    return result;
}

Poly Poly::zero() {
    double zeroCoeff[1] = {0.0};
    return Poly(zeroCoeff, 0);
}

Poly Poly::deflate(const Poly& P, double x) {
    int deg = P.degree - 1;
    double* newCoeffs = new double[deg + 1];
    newCoeffs[deg] = P.coeffs[P.degree];
    for (int i = deg - 1; i >= 0; i--)
        newCoeffs[i] = P.coeffs[i + 1] + x * newCoeffs[i + 1];
    Poly result(newCoeffs, deg);
    delete[] newCoeffs;
    return result;
}

double Poly::evalDerivative(const Poly& P, double x) {
    return P.derivative().evaluate(x);
}

void Poly::generateCSV(const Poly& P, double x1, double x2, int numPoints, const char* filename) {
    std::ofstream out(filename);
    double step = (x2 - x1) / (numPoints - 1);
    out << "x,y\n";
    for (int i = 0; i < numPoints; i++) {
        double x = x1 + i * step;
        out << x << "," << P.evaluate(x) << "\n";
    }
    out.close();
}

bool Poly::writeToFile(const char* filename) const {
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(&degree), sizeof(int));
    out.write(reinterpret_cast<const char*>(coeffs), sizeof(double) * (degree + 1));
    return true;
}

bool Poly::readFromFile(const char* filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) return false;
    int deg;
    in.read(reinterpret_cast<char*>(&deg), sizeof(int));
    double* buffer = new double[deg + 1];
    in.read(reinterpret_cast<char*>(buffer), sizeof(double) * (deg + 1));
    cleanup();
    coeffs = buffer;
    degree = deg;
    return true;
}
