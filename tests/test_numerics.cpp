// tests/test_numerics.cpp — roots, optimisation, integration, ODE, interpolation, statistics.
#include "cml_test.hpp"
#include "cml/core.hpp"
#include "cml/integrate.hpp"
#include "cml/interpolate.hpp"
#include "cml/ode.hpp"
#include "cml/optimize.hpp"
#include "cml/roots.hpp"
#include "cml/stats.hpp"

#include <cmath>
#include <stdexcept>

namespace {
const double SQRT2 = 1.4142135623730951;
}

TEST_CASE("roots: bracketing and open methods find sqrt(2)") {
    cml::RealFn f = [](double x) { return x * x - 2.0; };
    cml::RealFn df = [](double x) { return 2.0 * x; };
    cml::RealFn d2f = [](double) { return 2.0; };
    cml::RootOptions opt;
    opt.tol = 1e-14;
    for (const auto& r : {cml::bisection(f, 1.0, 2.0, opt), cml::regula_falsi(f, 1.0, 2.0, opt), cml::brent(f, 1.0, 2.0, opt),
                          cml::ridders(f, 1.0, 2.0, opt), cml::secant(f, 1.0, 2.0, opt), cml::newton(f, df, 1.0, opt),
                          cml::newton(f, 1.0, opt), cml::halley(f, df, d2f, 1.0, opt), cml::steffensen(f, 1.5, opt),
                          cml::linear_search(f, 0.0, 3.0, 0.1, opt), cml::variable_step(f, 0.0, 3.0, 0.01, opt)}) {
        ++cmltest::state().checks;
        if (!r.converged || std::fabs(r.root - SQRT2) > 1e-9)
            cmltest::report_failure(__FILE__, __LINE__, r.method + ": root=" + cmltest::show(r.root) + " converged=" +
                                                            (r.converged ? "yes" : "no") + " msg=" + r.message);
        CHECK(r.iterations > 0);
        CHECK(r.evaluations >= r.iterations);
        CHECK(!r.history.empty());
    }
    CHECK(cml::brent(f, 1.0, 2.0, opt).iterations < 12);
    CHECK(cml::newton(f, df, 1.0, opt).iterations < 8);
    CHECK(cml::bisection(f, 1.0, 2.0, opt).iterations > 40);
    // invalid bracket is reported, not thrown
    const cml::RootResult bad = cml::bisection(f, 2.0, 3.0);
    CHECK(!bad.converged);
    CHECK(bad.message.find("opposite signs") != std::string::npos);
    // swapped bracket is fine
    CHECK(cml::bisection(f, 2.0, 1.0).converged);
    // exact endpoint
    CHECK_EQ(cml::bisection(f, SQRT2, 2.0).iterations, 0);
    // fixed point: x = cos(x)
    const cml::RootResult fp = cml::fixed_point([](double x) { return cml::cos(x); }, 1.0);
    CHECK(fp.converged);
    CHECK_CLOSE(fp.root, 0.7390851332151607, 1e-9);
    // option validation throws
    cml::RootOptions badopt;
    badopt.tol = -1;
    CHECK_THROWS(cml::bisection(f, 1.0, 2.0, badopt), std::invalid_argument);
    CHECK_THROWS(cml::linear_search(f, 0.0, 1.0, -1.0), std::invalid_argument);
    CHECK_THROWS(cml::linear_search(f, 0.0, 1e9, 1e-6), std::invalid_argument);
    // zero derivative reported
    const cml::RootResult zd = cml::newton([](double x) { return x * x + 1; }, [](double) { return 0.0; }, 0.0);
    CHECK(!zd.converged);
    // history is capped
    cml::RootOptions capped;
    capped.max_history = 5;
    CHECK_EQ(cml::bisection(f, 1.0, 2.0, capped).history.size(), 5u);
    capped.record_history = false;
    CHECK(cml::bisection(f, 1.0, 2.0, capped).history.empty());
}

TEST_CASE("roots: find_all_roots and hard functions") {
    const auto all = cml::find_all_roots([](double x) { return cml::sin(x); }, 0.5, 10.0, 500);
    CHECK_EQ(all.size(), 3u);
    CHECK_CLOSE(all[0].root, cml::constants::pi, 1e-10);
    CHECK_CLOSE(all[1].root, 2 * cml::constants::pi, 1e-10);
    CHECK_CLOSE(all[2].root, 3 * cml::constants::pi, 1e-10);
    // poles (tan) are rejected
    const auto tan_roots = cml::find_all_roots([](double x) { return cml::tan(x); }, 0.5, 6.0, 1000);
    CHECK_EQ(tan_roots.size(), 1u);
    CHECK_CLOSE(tan_roots[0].root, cml::constants::pi, 1e-9);
    // Newton on x = cos(x)
    const cml::RootResult nr = cml::newton([](double x) { return cml::cos(x) - x; }, 0.5);
    CHECK(nr.converged);
    CHECK_CLOSE(nr.root, 0.7390851332151607, 1e-12);
    // Brent on a function with a flat region
    const cml::RootResult br = cml::brent([](double x) { return (x - 1.0) * (x - 1.0) * (x - 1.0); }, 0.0, 3.0);
    CHECK(br.converged);
    CHECK_CLOSE_ABS(br.root, 1.0, 1e-4);
    // non-finite function values are reported
    const cml::RootResult nf = cml::bisection([](double x) { return cml::ln(x); }, -1.0, 2.0);
    CHECK(!nf.converged);
    CHECK_THROWS(cml::find_all_roots([](double x) { return x; }, 0, 1, 1), std::invalid_argument);
}

TEST_CASE("optimize: one-dimensional minimisers") {
    cml::RealFn f = [](double x) { return (x - 2.0) * (x - 2.0) + 1.0; };
    cml::RealFn df = [](double x) { return 2.0 * (x - 2.0); };
    cml::RealFn d2f = [](double) { return 2.0; };
    cml::MinOptions opt;
    opt.tol = 1e-10;
    for (const auto& r : {cml::golden_section(f, 0.0, 5.0, opt), cml::fibonacci_search(f, 0.0, 5.0, opt),
                          cml::brent_minimize(f, 0.0, 5.0, opt), cml::binary_search_min(f, 0.0, 5.0, opt),
                          cml::newton_minimize(f, df, d2f, 0.0, opt), cml::newton_minimize(f, 0.0, opt),
                          cml::linear_search_min(f, 0.0, 5.0, 0.1, opt), cml::variable_step_min(f, 0.0, 5.0, 0.01, opt)}) {
        ++cmltest::state().checks;
        if (!r.converged || std::fabs(r.x - 2.0) > 1e-6 || std::fabs(r.fx - 1.0) > 1e-9)
            cmltest::report_failure(__FILE__, __LINE__, r.method + ": x=" + cmltest::show(r.x) + " fx=" + cmltest::show(r.fx) +
                                                            " converged=" + (r.converged ? "yes" : "no") + " msg=" + r.message);
        CHECK(r.evaluations > 0);
    }
    CHECK(cml::brent_minimize(f, 0.0, 5.0, opt).iterations < 60);
    const cml::Bracket b = cml::bracket_minimum(f, -10.0, 0.5);
    CHECK(b.found);
    CHECK(b.a < 2.0 && b.c > 2.0);
    // Newton reports a maximum as non-converged
    const cml::MinResult mx = cml::newton_minimize([](double x) { return -x * x; }, 0.5, opt);
    CHECK(!mx.converged);
    // non-smooth function still works with bracketing methods
    const cml::MinResult ab = cml::golden_section([](double x) { return cml::abs(x - 1.5); }, 0.0, 4.0, opt);
    CHECK_CLOSE_ABS(ab.x, 1.5, 1e-8);
    // multi-modal: sin(x) + sin(10x/3) on [2.7, 7.5], global min near 5.1457
    const cml::MinResult mm = cml::brent_minimize([](double x) { return cml::sin(x) + cml::sin(10.0 * x / 3.0); }, 4.0, 6.0, opt);
    CHECK_CLOSE_ABS(mm.x, 5.145735, 1e-5);
    CHECK_THROWS(cml::golden_section(f, 0.0, 1.0, cml::MinOptions{-1.0, 10, true, 10}), std::invalid_argument);
}

TEST_CASE("optimize: multi-dimensional") {
    cml::VecFn rosen = [](const std::vector<double>& p) {
        const double x = p[0], y = p[1];
        return (1 - x) * (1 - x) + 100 * (y - x * x) * (y - x * x);
    };
    cml::MinOptionsN opt;
    opt.tol = 1e-10;
    opt.max_iter = 5000;
    const cml::MinResultN nm = cml::nelder_mead(rosen, {-1.2, 1.0}, 0.5, opt);
    CHECK(nm.converged);
    CHECK_CLOSE_ABS(nm.x[0], 1.0, 1e-4);
    CHECK_CLOSE_ABS(nm.x[1], 1.0, 1e-4);
    CHECK(nm.fx < 1e-8);
    CHECK(!nm.path.empty());
    CHECK_EQ(nm.path.size(), nm.path_fx.size());
    cml::VecFn quad = [](const std::vector<double>& p) { return (p[0] - 3) * (p[0] - 3) + 2 * (p[1] + 1) * (p[1] + 1); };
    const cml::MinResultN gd = cml::gradient_descent(quad, {0.0, 0.0}, 0.1, opt);
    CHECK(gd.converged);
    CHECK_CLOSE_ABS(gd.x[0], 3.0, 1e-6);
    CHECK_CLOSE_ABS(gd.x[1], -1.0, 1e-6);
    cml::MinOptionsN few;
    few.max_iter = 20000;
    few.tol = 1e-8;
    const cml::MinResultN gdr = cml::gradient_descent(rosen, {-1.2, 1.0}, 0.01, few);
    CHECK(gdr.fx < 1e-3);
    const auto g = cml::numeric_gradient(quad, {0.0, 0.0});
    CHECK_CLOSE_ABS(g[0], -6.0, 1e-6);
    CHECK_CLOSE_ABS(g[1], 4.0, 1e-6);
    CHECK_THROWS(cml::nelder_mead(quad, {}), std::invalid_argument);
    CHECK_THROWS(cml::gradient_descent(quad, {0.0}, -1.0), std::invalid_argument);
    // 5-D sphere
    cml::VecFn sphere = [](const std::vector<double>& p) { double s = 0; for (double v : p) s += (v - 1) * (v - 1); return s; };
    const cml::MinResultN s5 = cml::nelder_mead(sphere, std::vector<double>(5, 0.0), 1.0, opt);
    CHECK(s5.fx < 1e-10);
}

TEST_CASE("integrate: quadrature rules") {
    cml::RealFn s = [](double x) { return cml::sin(x); };
    const double a = 0.0, b = cml::constants::pi;
    CHECK_CLOSE(cml::midpoint(s, a, b, 1000).value, 2.0, 1e-5);
    CHECK_CLOSE(cml::trapezoid(s, a, b, 1000).value, 2.0, 1e-5);
    CHECK_CLOSE(cml::simpson(s, a, b, 100).value, 2.0, 1e-8);
    CHECK_CLOSE(cml::simpson(s, a, b, 101).value, 2.0, 1e-8);     // odd n is rounded up
    CHECK_CLOSE(cml::simpson38(s, a, b, 99).value, 2.0, 1e-7);
    CHECK_CLOSE(cml::boole(s, a, b, 40).value, 2.0, 1e-9);
    const cml::IntegralResult rom = cml::romberg(s, a, b);
    CHECK(rom.converged);
    CHECK_CLOSE(rom.value, 2.0, 1e-13);
    CHECK(rom.table.size() >= 4);
    CHECK_CLOSE(cml::gauss_legendre(s, a, b, 10).value, 2.0, 1e-13);
    CHECK_CLOSE(cml::gauss_legendre([](double x) { return x * x * x * x * x; }, 0, 1, 3).value, 1.0 / 6.0, 1e-15); // exact for degree <= 2n-1
    CHECK_CLOSE(cml::gauss_legendre([](double) { return 1.0; }, 0, 1, 1).value, 1.0, 1e-15);
    CHECK_CLOSE(cml::gauss_legendre(s, a, b, 64).value, 2.0, 1e-13);
    CHECK_THROWS(cml::gauss_legendre(s, a, b, 65), std::invalid_argument);
    const cml::IntegralResult ad = cml::adaptive_simpson(s, a, b, 1e-12);
    CHECK(ad.converged);
    CHECK_CLOSE(ad.value, 2.0, 1e-11);
    const cml::IntegralResult mc = cml::monte_carlo(s, a, b, 200000);
    CHECK(std::fabs(mc.value - 2.0) < 5 * mc.error_estimate + 1e-3);
    CHECK_EQ(cml::monte_carlo(s, a, b, 1000, 7).value, cml::monte_carlo(s, a, b, 1000, 7).value); // deterministic
    // improper integrals
    cml::RealFn gauss = [](double x) { return cml::exp(-x * x); };
    const double inf = cml::inf();
    CHECK_CLOSE(cml::integrate(gauss, -inf, inf).value, cml::sqrt(cml::constants::pi), 1e-8);
    CHECK_CLOSE(cml::integrate(gauss, 0.0, inf).value, 0.5 * cml::sqrt(cml::constants::pi), 1e-8);
    CHECK_CLOSE(cml::integrate([](double x) { return cml::exp(x); }, -inf, 0.0).value, 1.0, 1e-8);
    CHECK_CLOSE(cml::integrate(s, b, a).value, -2.0, 1e-9);        // reversed bounds flip sign
    CHECK_EQ(cml::integrate(s, 1.0, 1.0).value, 0.0);
    // integrable singularity 1/sqrt(x) on [0, 1] = 2
    CHECK_CLOSE(cml::integrate([](double x) { return x > 0 ? 1.0 / cml::sqrt(x) : 0.0; }, 0.0, 1.0, 1e-8).value, 2.0, 1e-3);
    CHECK_THROWS(cml::simpson(s, a, b, 0), std::invalid_argument);
    CHECK_THROWS(cml::romberg(s, a, b, 50), std::invalid_argument);
    std::vector<double> nodes, weights;
    cml::gauss_legendre_nodes(5, nodes, weights);
    double wsum = 0.0;
    for (double w : weights) wsum += w;
    CHECK_CLOSE(wsum, 2.0, 1e-14);
    CHECK_CLOSE_ABS(nodes[2], 0.0, 1e-15);
    CHECK_CLOSE(nodes[4], 0.9061798459386640, 1e-14);
}

TEST_CASE("ode: exponential growth and oscillator") {
    cml::OdeFn growth = [](double, const std::vector<double>& y) { return std::vector<double>{y[0]}; };
    const cml::OdeResult e1 = cml::euler(growth, 0.0, {1.0}, 1.0, 0.001);
    CHECK(e1.success);
    CHECK_CLOSE(e1.y.back()[0], cml::constants::e, 2e-3);
    CHECK_EQ(e1.steps, 1000);
    CHECK_EQ(e1.t.back(), 1.0);
    CHECK_CLOSE(cml::heun(growth, 0.0, {1.0}, 1.0, 0.01).y.back()[0], cml::constants::e, 1e-4);
    CHECK_CLOSE(cml::midpoint_method(growth, 0.0, {1.0}, 1.0, 0.01).y.back()[0], cml::constants::e, 1e-4);
    const cml::OdeResult r4 = cml::rk4(growth, 0.0, {1.0}, 1.0, 0.01);
    CHECK_CLOSE(r4.y.back()[0], cml::constants::e, 1e-9);
    CHECK_EQ(r4.evaluations, 400);
    cml::OdeOptions tight;
    tight.tol = 1e-12;
    const cml::OdeResult r45 = cml::rk45(growth, 0.0, {1.0}, 1.0, 0.0, tight);
    CHECK(r45.success);
    CHECK_CLOSE(r45.y.back()[0], cml::constants::e, 1e-10);
    CHECK(r45.steps < 200);
    CHECK_EQ(r45.t.back(), 1.0);
    // backwards in time
    const cml::OdeResult back = cml::rk4(growth, 1.0, {cml::constants::e}, 0.0, 0.01);
    CHECK_CLOSE(back.y.back()[0], 1.0, 1e-9);
    // harmonic oscillator: y'' = -y  ->  (y, v)' = (v, -y); energy conserved by rk4
    cml::OdeFn osc = [](double, const std::vector<double>& y) { return std::vector<double>{y[1], -y[0]}; };
    const cml::OdeResult o = cml::rk4(osc, 0.0, {1.0, 0.0}, 2 * cml::constants::pi, 0.01);
    CHECK_CLOSE(o.y.back()[0], 1.0, 1e-7);
    CHECK_CLOSE_ABS(o.y.back()[1], 0.0, 1e-7);
    const cml::OdeResult o45 = cml::rk45(osc, 0.0, {1.0, 0.0}, 20.0, 0.0, tight);
    CHECK(o45.success);
    CHECK_CLOSE(o45.y.back()[0], cml::cos(20.0), 1e-8);
    // step limit and non-finite detection are reported, not thrown
    cml::OdeOptions few;
    few.max_steps = 10;
    CHECK(!cml::rk4(growth, 0.0, {1.0}, 1.0, 0.01, few).success);
    const cml::OdeResult blow = cml::euler([](double, const std::vector<double>& y) { return std::vector<double>{y[0] * y[0]}; }, 0.0, {1.0}, 5.0, 0.1);
    CHECK(!blow.success);
    CHECK_THROWS(cml::rk4(growth, 0.0, {}, 1.0, 0.1), std::invalid_argument);
    CHECK_THROWS(cml::rk4(growth, 0.0, {1.0}, 1.0, 0.0), std::invalid_argument);
    const cml::OdeResult sc = cml::solve_scalar("rk4", [](double t, double y) { return t * y; }, 0.0, 1.0, 1.0, 0.01);
    CHECK_CLOSE(sc.y.back()[0], cml::exp(0.5), 1e-8);
    CHECK_THROWS(cml::solve_scalar("bogus", [](double, double) { return 0.0; }, 0, 0, 1, 0.1), std::invalid_argument);
}

TEST_CASE("interpolate: lagrange, newton, spline, polyfit") {
    const std::vector<double> xs{0, 1, 2, 3}, ys{1, 2, 5, 10}; // y = x^2 + 1
    CHECK_CLOSE(cml::lagrange_interpolate(xs, ys, 1.5), 3.25, 1e-14);
    CHECK_EQ(cml::lagrange_polynomial(xs, ys).to_string(), std::string("x^2 + 1"));
    const cml::NewtonInterpolation nd = cml::newton_divided_differences(xs, ys);
    CHECK_EQ(nd.polynomial.to_string(), std::string("x^2 + 1"));
    CHECK_CLOSE(nd.evaluate(2.5), 7.25, 1e-14);
    CHECK_EQ(nd.table.size(), 4u);
    CHECK_EQ(nd.coefficients[2], 1.0);
    CHECK_EQ(nd.coefficients[3], 0.0);
    CHECK_CLOSE(cml::linear_interpolate(xs, ys, 1.5), 3.5, 1e-15);
    CHECK_CLOSE(cml::linear_interpolate({2, 0, 1}, {5, 1, 2}, 1.5), 3.5, 1e-15); // unsorted input ok
    CHECK_THROWS(cml::lagrange_polynomial({1, 1}, {1, 2}), std::invalid_argument);
    CHECK_THROWS(cml::lagrange_interpolate({1, 2}, {1}, 0.0), std::invalid_argument);
    const cml::CubicSpline sp = cml::CubicSpline::natural(xs, ys);
    for (std::size_t i = 0; i < xs.size(); ++i) CHECK_CLOSE_ABS(sp(xs[i]), ys[i], 1e-12);
    CHECK_CLOSE_ABS(sp.second_derivative(0.0), 0.0, 1e-12);     // natural end conditions
    CHECK_CLOSE_ABS(sp.second_derivative(3.0), 0.0, 1e-12);
    CHECK_CLOSE(sp.derivative(1.0 + 1e-9), sp.derivative(1.0 - 1e-9), 1e-6); // C1 continuity
    CHECK_EQ(sp.segments().size(), 3u);
    const cml::CubicSpline cl = cml::CubicSpline::clamped(xs, ys, 0.0, 6.0); // exact derivatives of x^2+1
    for (double x : {0.5, 1.7, 2.9}) CHECK_CLOSE(cl(x), x * x + 1, 1e-12);
    CHECK_CLOSE(cl.integral(0.0, 3.0), 9.0 + 3.0, 1e-12);
    CHECK_CLOSE(cl.derivative(1.5), 3.0, 1e-12);
    // least squares fit recovers an exact quadratic and reports r^2 = 1
    const cml::FitResult fit = cml::polyfit({-2, -1, 0, 1, 2, 3}, {5, 2, 1, 2, 5, 10}, 2);
    CHECK_CLOSE(fit.polynomial[2], 1.0, 1e-12);
    CHECK_CLOSE_ABS(fit.polynomial[1], 0.0, 1e-12);
    CHECK_CLOSE(fit.polynomial[0], 1.0, 1e-12);
    CHECK_CLOSE(fit.r_squared, 1.0, 1e-12);
    CHECK(fit.rmse < 1e-12);
    const cml::FitResult noisy = cml::polyfit({1, 2, 3, 4, 5}, {2.1, 3.9, 6.2, 7.8, 10.1}, 1);
    CHECK_CLOSE(noisy.polynomial[1], 2.0, 0.05);
    CHECK(noisy.r_squared > 0.99);
    CHECK_THROWS(cml::polyfit({1, 2}, {1, 2}, 2), std::invalid_argument);
    CHECK_THROWS(cml::polyfit({1, 2}, {1, 2}, 60), std::invalid_argument);
    // large-x conditioning
    const cml::FitResult far = cml::polyfit({1000, 1001, 1002, 1003}, {1, 2, 5, 10}, 2);
    CHECK_CLOSE(far.polynomial.evaluate(1001.5), 3.25, 1e-6);
}

TEST_CASE("stats: descriptive, correlation, regression, histogram") {
    const std::vector<double> d{2, 4, 4, 4, 5, 5, 7, 9};
    const cml::Descriptive s = cml::describe(d);
    CHECK_EQ(s.count, 8u);
    CHECK_EQ(s.mean, 5.0);
    CHECK_EQ(s.sum, 40.0);
    CHECK_EQ(s.median, 4.5);
    CHECK_EQ(s.mode, 4.0);
    CHECK_EQ(s.min, 2.0);
    CHECK_EQ(s.max, 9.0);
    CHECK_EQ(s.stddev_population, 2.0);
    CHECK_CLOSE(s.variance, 32.0 / 7.0, 1e-15);
    CHECK_EQ(s.q1, 4.0);
    CHECK_EQ(s.q3, 5.5);
    CHECK_EQ(s.iqr, 1.5);
    CHECK(s.skewness > 0.0);
    CHECK_CLOSE(s.geometric_mean, 4.6032155960467369, 1e-12);
    CHECK_CLOSE(s.harmonic_mean, 4.2017507294706133, 1e-12);
    CHECK_EQ(cml::quantile(d, 0.0), 2.0);
    CHECK_EQ(cml::quantile(d, 1.0), 9.0);
    CHECK(cml::is_nan(cml::mode({1, 2, 3})));
    CHECK_THROWS(cml::describe({}), std::invalid_argument);
    CHECK_THROWS(cml::variance({1.0}), std::invalid_argument);
    CHECK_THROWS(cml::mean({1.0, NAN}), std::invalid_argument);
    const std::vector<double> x{1, 2, 3, 4, 5}, y{2, 4, 6, 8, 10};
    CHECK_CLOSE(cml::correlation(x, y), 1.0, 1e-15);
    CHECK_CLOSE(cml::covariance(x, y), 5.0, 1e-15);
    CHECK_CLOSE(cml::spearman(x, {5, 4, 3, 2, 1}), -1.0, 1e-15);
    const auto rk = cml::ranks({10, 20, 20, 30});
    CHECK_EQ(rk[1], 2.5);
    CHECK_EQ(rk[3], 4.0);
    const cml::LinearRegression lr = cml::linear_regression(x, {2.1, 3.9, 6.2, 7.8, 10.1});
    CHECK_CLOSE(lr.slope, 2.0, 0.05);
    CHECK_CLOSE_ABS(lr.intercept, 0.0, 0.2);
    CHECK(lr.r_squared > 0.99);
    CHECK_EQ(lr.residuals.size(), 5u);
    CHECK(lr.std_error_slope > 0.0);
    CHECK_THROWS(cml::linear_regression({1, 1, 1}, {1, 2, 3}), std::invalid_argument);
    const cml::Histogram h = cml::histogram(d, 4);
    CHECK_EQ(h.counts.size(), 4u);
    CHECK_EQ(h.edges.size(), 5u);
    std::size_t total = 0;
    for (std::size_t c : h.counts) total += c;
    CHECK_EQ(total, 8u);
    CHECK_EQ(h.counts[3], 1u);   // the 9
    CHECK_EQ(cml::histogram(d).counts.size(), 4u); // Sturges: ceil(log2(8)+1) = 4
    const auto z = cml::z_scores(d);
    double zsum = 0.0;
    for (double v : z) zsum += v;
    CHECK_CLOSE_ABS(zsum, 0.0, 1e-12);
    const auto ma = cml::moving_average({1, 2, 3, 4, 5}, 3);
    CHECK_EQ(ma.size(), 3u);
    CHECK_EQ(ma[0], 2.0);
    CHECK_EQ(ma[2], 4.0);
    CHECK_EQ(cml::cumulative_sum({1, 2, 3})[2], 6.0);
    CHECK_EQ(cml::min_max_normalize({2, 4, 6})[1], 0.5);
}
