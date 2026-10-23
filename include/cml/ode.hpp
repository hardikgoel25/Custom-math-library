// cml/ode.hpp — initial value problem solvers for y' = f(t, y), y in R^n.
#ifndef CML_ODE_HPP
#define CML_ODE_HPP

#include <functional>
#include <string>
#include <vector>

namespace cml {

using OdeFn = std::function<std::vector<double>(double t, const std::vector<double>& y)>;

struct OdeOptions {
    double tol = 1e-8;        // adaptive methods: relative/absolute tolerance
    double h_min = 1e-12;
    double h_max = 0.0;       // 0 = unlimited
    int max_steps = 200000;
};

struct OdeResult {
    std::vector<double> t;
    std::vector<std::vector<double>> y;  // y[i] is the state at t[i]
    int steps = 0;
    int rejected = 0;
    int evaluations = 0;
    bool success = false;
    std::string method;
    std::string message;
};

OdeResult euler(const OdeFn& f, double t0, const std::vector<double>& y0, double t1, double h, const OdeOptions& opt = OdeOptions());
OdeResult heun(const OdeFn& f, double t0, const std::vector<double>& y0, double t1, double h, const OdeOptions& opt = OdeOptions());
OdeResult midpoint_method(const OdeFn& f, double t0, const std::vector<double>& y0, double t1, double h, const OdeOptions& opt = OdeOptions());
OdeResult rk4(const OdeFn& f, double t0, const std::vector<double>& y0, double t1, double h, const OdeOptions& opt = OdeOptions());
OdeResult rk45(const OdeFn& f, double t0, const std::vector<double>& y0, double t1, double h0 = 0.0, const OdeOptions& opt = OdeOptions()); // Dormand–Prince adaptive

// Scalar convenience wrappers.
OdeResult solve_scalar(const std::string& method, const std::function<double(double, double)>& f, double t0, double y0,
                       double t1, double h, const OdeOptions& opt = OdeOptions());

} // namespace cml

#endif // CML_ODE_HPP
