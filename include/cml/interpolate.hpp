// cml/interpolate.hpp — interpolation and least-squares fitting.
#ifndef CML_INTERPOLATE_HPP
#define CML_INTERPOLATE_HPP

#include "cml/polynomial.hpp"

#include <string>
#include <vector>

namespace cml {

// Lagrange ------------------------------------------------------------------
double lagrange_interpolate(const std::vector<double>& xs, const std::vector<double>& ys, double x);
Polynomial lagrange_polynomial(const std::vector<double>& xs, const std::vector<double>& ys);

// Newton divided differences --------------------------------------------------
struct NewtonInterpolation {
    std::vector<double> xs;
    std::vector<double> coefficients;             // c0, c1, ... (Newton form)
    std::vector<std::vector<double>> table;       // divided-difference table, table[k][i]
    Polynomial polynomial;                        // expanded into monomial basis
    double evaluate(double x) const;
};
NewtonInterpolation newton_divided_differences(const std::vector<double>& xs, const std::vector<double>& ys);

// Piecewise linear ----------------------------------------------------------
double linear_interpolate(const std::vector<double>& xs, const std::vector<double>& ys, double x);

// Cubic spline --------------------------------------------------------------
class CubicSpline {
public:
    struct Segment { double x0, x1, a, b, c, d; }; // S(x) = a + b(x-x0) + c(x-x0)^2 + d(x-x0)^3
    static CubicSpline natural(const std::vector<double>& xs, const std::vector<double>& ys);
    static CubicSpline clamped(const std::vector<double>& xs, const std::vector<double>& ys, double d0, double dn);
    double operator()(double x) const;
    double derivative(double x) const;
    double second_derivative(double x) const;
    double integral(double a, double b) const;
    const std::vector<Segment>& segments() const { return segments_; }
private:
    std::vector<Segment> segments_;
    const Segment& locate(double x) const;
    static CubicSpline build(const std::vector<double>& xs, const std::vector<double>& ys, bool clamped, double d0, double dn);
};

// Least squares polynomial fit -----------------------------------------------
struct FitResult {
    Polynomial polynomial;
    double r_squared = 0.0;
    double rmse = 0.0;
    std::vector<double> residuals;
};
FitResult polyfit(const std::vector<double>& xs, const std::vector<double>& ys, int degree);

// Helpers ------------------------------------------------------------------
void sort_points(std::vector<double>& xs, std::vector<double>& ys); // sorts by x, throws on duplicate x

} // namespace cml

#endif // CML_INTERPOLATE_HPP
