// cml/optimize.hpp — one-dimensional and multi-dimensional minimisation.
//
// All routines minimise; to maximise, pass the negated function.  Results
// carry iteration histories (1-D) or paths (N-D) so the UI can animate them.
#ifndef CML_OPTIMIZE_HPP
#define CML_OPTIMIZE_HPP

#include <functional>
#include <string>
#include <vector>

namespace cml {

using RealFn = std::function<double(double)>;
using VecFn = std::function<double(const std::vector<double>&)>;

struct MinStep {
    int iteration;
    double a, b;    // current bracket (or previous/current iterate for open methods)
    double x, fx;   // current best estimate
};

struct MinResult {
    double x = 0.0;
    double fx = 0.0;
    int iterations = 0;
    int evaluations = 0;
    bool converged = false;
    std::string method;
    std::string message;
    std::vector<MinStep> history;
};

struct MinOptions {
    double tol = 1e-8;
    int max_iter = 500;
    bool record_history = true;
    int max_history = 2000;
};

struct Bracket {
    double a = 0.0, b = 0.0, c = 0.0;     // a < b < c with f(b) < f(a), f(b) < f(c)
    double fa = 0.0, fb = 0.0, fc = 0.0;
    int evaluations = 0;
    bool found = false;
};

Bracket bracket_minimum(const RealFn& f, double x0, double step = 0.1, int max_iter = 200);

MinResult golden_section(const RealFn& f, double a, double b, const MinOptions& opt = MinOptions());
MinResult fibonacci_search(const RealFn& f, double a, double b, const MinOptions& opt = MinOptions());
MinResult brent_minimize(const RealFn& f, double a, double b, const MinOptions& opt = MinOptions());
MinResult newton_minimize(const RealFn& f, const RealFn& df, const RealFn& d2f, double x0, const MinOptions& opt = MinOptions());
MinResult newton_minimize(const RealFn& f, double x0, const MinOptions& opt = MinOptions()); // numeric derivatives
MinResult binary_search_min(const RealFn& f, double a, double b, const MinOptions& opt = MinOptions()); // bisection on f'
MinResult linear_search_min(const RealFn& f, double a, double b, double step, const MinOptions& opt = MinOptions());
MinResult variable_step_min(const RealFn& f, double a, double b, double initial_step, const MinOptions& opt = MinOptions());

// Multi-dimensional -----------------------------------------------------------
struct MinResultN {
    std::vector<double> x;
    double fx = 0.0;
    int iterations = 0;
    int evaluations = 0;
    bool converged = false;
    std::string method;
    std::string message;
    std::vector<std::vector<double>> path; // best point per iteration
    std::vector<double> path_fx;
};

struct MinOptionsN {
    double tol = 1e-8;
    int max_iter = 2000;
    bool record_path = true;
    int max_path = 5000;
};

std::vector<double> numeric_gradient(const VecFn& f, const std::vector<double>& x, double h = 1e-6);
MinResultN nelder_mead(const VecFn& f, const std::vector<double>& x0, double initial_step = 1.0,
                       const MinOptionsN& opt = MinOptionsN());
MinResultN gradient_descent(const VecFn& f, const std::vector<double>& x0, double learning_rate = 0.1,
                            const MinOptionsN& opt = MinOptionsN());
MinResultN gradient_descent(const VecFn& f, const std::function<std::vector<double>(const std::vector<double>&)>& grad,
                            const std::vector<double>& x0, double learning_rate, const MinOptionsN& opt = MinOptionsN());

} // namespace cml

#endif // CML_OPTIMIZE_HPP
