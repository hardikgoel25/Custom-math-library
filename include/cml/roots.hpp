// cml/roots.hpp — scalar root finding with full iteration histories.
//
// Every method returns a RootResult describing what happened (root, residual,
// iteration/evaluation counts, convergence flag, human-readable message and an
// optional per-iteration history that UIs can plot or tabulate).  Nothing is
// printed and nothing throws for numerical failure; invalid arguments throw
// std::invalid_argument.
#ifndef CML_ROOTS_HPP
#define CML_ROOTS_HPP

#include <functional>
#include <string>
#include <vector>

namespace cml {

using RealFn = std::function<double(double)>;

struct RootStep {
    int iteration;
    double a;      // bracket low (or previous iterate for open methods)
    double b;      // bracket high (or current iterate)
    double x;      // current estimate
    double fx;     // f(x)
    double error;  // |x_k - x_{k-1}| or bracket width
};

struct RootResult {
    double root = 0.0;
    double f_root = 0.0;
    int iterations = 0;
    int evaluations = 0;
    bool converged = false;
    std::string method;
    std::string message;
    std::vector<RootStep> history;
};

struct RootOptions {
    double tol = 1e-12;          // absolute tolerance on x (and on |f| where meaningful)
    int max_iter = 200;
    bool record_history = true;
    int max_history = 2000;
};

RootResult bisection(const RealFn& f, double a, double b, const RootOptions& opt = RootOptions());
RootResult regula_falsi(const RealFn& f, double a, double b, const RootOptions& opt = RootOptions()); // Illinois variant
RootResult secant(const RealFn& f, double x0, double x1, const RootOptions& opt = RootOptions());
RootResult newton(const RealFn& f, const RealFn& df, double x0, const RootOptions& opt = RootOptions());
RootResult newton(const RealFn& f, double x0, const RootOptions& opt = RootOptions());  // numerical derivative
RootResult halley(const RealFn& f, const RealFn& df, const RealFn& d2f, double x0, const RootOptions& opt = RootOptions());
RootResult brent(const RealFn& f, double a, double b, const RootOptions& opt = RootOptions());
RootResult ridders(const RealFn& f, double a, double b, const RootOptions& opt = RootOptions());
RootResult steffensen(const RealFn& f, double x0, const RootOptions& opt = RootOptions());
RootResult fixed_point(const RealFn& g, double x0, const RootOptions& opt = RootOptions()); // x = g(x)
RootResult linear_search(const RealFn& f, double a, double b, double step, const RootOptions& opt = RootOptions());
RootResult variable_step(const RealFn& f, double a, double b, double initial_step, const RootOptions& opt = RootOptions());

// Scan [a, b] with `samples` points, bracket every sign change and refine with Brent.
std::vector<RootResult> find_all_roots(const RealFn& f, double a, double b, int samples = 1000,
                                       const RootOptions& opt = RootOptions());

} // namespace cml

#endif // CML_ROOTS_HPP
