// cml/polynomial.hpp — dense univariate polynomials with real coefficients.
//
// Coefficients are stored lowest degree first: p(x) = c[0] + c[1] x + ... + c[n] x^n.
#ifndef CML_POLYNOMIAL_HPP
#define CML_POLYNOMIAL_HPP

#include <complex>
#include <initializer_list>
#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

namespace cml {

class Polynomial {
public:
    using Complex = std::complex<double>;

    Polynomial();                                        // zero polynomial
    Polynomial(std::vector<double> coefficients);        // lowest degree first
    Polynomial(std::initializer_list<double> coefficients);
    static Polynomial from_roots(const std::vector<double>& roots, double leading = 1.0);
    static Polynomial monomial(int degree, double coefficient = 1.0);
    static Polynomial parse(const std::string& text, char var = 'x'); // "3x^2 - 2x + 1"

    int degree() const;                          // -1 for the zero polynomial
    bool is_zero() const;
    bool is_monic() const;
    std::size_t size() const { return c_.size(); }
    double leading() const;
    double coefficient(int exponent) const;      // 0 outside range
    double operator[](int exponent) const { return coefficient(exponent); }
    void set(int exponent, double value);        // grows as needed (exponent >= 0)
    const std::vector<double>& coefficients() const { return c_; }
    Polynomial trimmed(double epsilon = 0.0) const; // drop tiny leading coefficients
    Polynomial cleaned(double rel_eps = 1e-12) const; // zero coefficients tiny relative to the largest

    double  evaluate(double x) const;            // Horner
    Complex evaluate(Complex z) const;
    std::pair<double, double> evaluate_with_derivative(double x) const;
    double operator()(double x) const { return evaluate(x); }

    Polynomial derivative() const;
    Polynomial integral(double constant = 0.0) const;
    double definite_integral(double a, double b) const;
    Polynomial compose(const Polynomial& inner) const;   // this(inner(x))
    Polynomial pow(unsigned n) const;
    Polynomial normalized() const;                       // monic
    Polynomial deflate(double root) const;               // synthetic division by (x - root)
    Polynomial shifted(double h) const;                  // p(x + h)

    Polynomial operator-() const;
    Polynomial& operator+=(const Polynomial& o);
    Polynomial& operator-=(const Polynomial& o);
    Polynomial& operator*=(const Polynomial& o);
    Polynomial& operator*=(double s);
    Polynomial& operator/=(double s);

    static std::pair<Polynomial, Polynomial> divmod(const Polynomial& a, const Polynomial& b); // {quotient, remainder}
    static Polynomial gcd(Polynomial a, Polynomial b, double epsilon = 1e-10);

    // Roots -------------------------------------------------------------
    struct RootsResult {
        std::vector<Complex> roots;   // all complex roots (with multiplicity)
        std::vector<double> real_roots;
        int iterations = 0;
        bool converged = false;
    };
    RootsResult roots(double tol = 1e-14, int max_iter = 500) const; // Aberth–Ehrlich + Newton polish
    std::vector<double> real_roots(double tol = 1e-14) const;

    std::string to_string(char var = 'x', int precision = 10) const;
    std::string to_unicode(char var = 'x', int precision = 10) const; // uses superscripts

private:
    std::vector<double> c_;
    void trim_inplace(double epsilon = 0.0);
};

Polynomial operator+(const Polynomial& a, const Polynomial& b);
Polynomial operator-(const Polynomial& a, const Polynomial& b);
Polynomial operator*(const Polynomial& a, const Polynomial& b);
Polynomial operator*(const Polynomial& a, double s);
Polynomial operator*(double s, const Polynomial& a);
Polynomial operator/(const Polynomial& a, double s);
Polynomial operator/(const Polynomial& a, const Polynomial& b); // quotient
Polynomial operator%(const Polynomial& a, const Polynomial& b); // remainder
bool operator==(const Polynomial& a, const Polynomial& b);
bool operator!=(const Polynomial& a, const Polynomial& b);
std::ostream& operator<<(std::ostream& os, const Polynomial& p);

} // namespace cml

#endif // CML_POLYNOMIAL_HPP
