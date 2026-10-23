// cml/integrate.hpp — numerical quadrature.
#ifndef CML_INTEGRATE_HPP
#define CML_INTEGRATE_HPP

#include <functional>
#include <string>
#include <vector>

namespace cml {

using RealFn = std::function<double(double)>;

struct IntegralResult {
    double value = 0.0;
    double error_estimate = 0.0;   // where the method provides one (0 otherwise)
    int evaluations = 0;
    int subdivisions = 0;          // intervals, levels, or samples depending on the method
    bool converged = true;
    std::string method;
    std::string message;
    std::vector<std::vector<double>> table; // Romberg tableau
};

IntegralResult midpoint(const RealFn& f, double a, double b, int n = 100);
IntegralResult trapezoid(const RealFn& f, double a, double b, int n = 100);
IntegralResult simpson(const RealFn& f, double a, double b, int n = 100);      // n rounded up to even
IntegralResult simpson38(const RealFn& f, double a, double b, int n = 99);     // n rounded up to a multiple of 3
IntegralResult boole(const RealFn& f, double a, double b, int n = 100);        // n rounded up to a multiple of 4
IntegralResult romberg(const RealFn& f, double a, double b, int max_levels = 20, double tol = 1e-12);
IntegralResult gauss_legendre(const RealFn& f, double a, double b, int points = 10); // 1..64 points
IntegralResult adaptive_simpson(const RealFn& f, double a, double b, double tol = 1e-10, int max_depth = 50);
IntegralResult monte_carlo(const RealFn& f, double a, double b, int samples = 100000, unsigned long long seed = 42);

// General-purpose: handles infinite bounds by substitution and uses adaptive Simpson.
IntegralResult integrate(const RealFn& f, double a, double b, double tol = 1e-10);

// Gauss–Legendre nodes/weights on [-1, 1] (computed once per n and cached).
void gauss_legendre_nodes(int n, std::vector<double>& nodes, std::vector<double>& weights);

} // namespace cml

#endif // CML_INTEGRATE_HPP
