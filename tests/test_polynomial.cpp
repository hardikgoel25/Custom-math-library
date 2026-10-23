// tests/test_polynomial.cpp
#include "cml_test.hpp"
#include "cml/polynomial.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

using cml::Polynomial;

TEST_CASE("polynomial: construction, parsing, printing") {
    Polynomial p({1.0, -2.0, 3.0}); // 3x^2 - 2x + 1
    CHECK_EQ(p.degree(), 2);
    CHECK_EQ(p.leading(), 3.0);
    CHECK_EQ(p[0], 1.0);
    CHECK_EQ(p[5], 0.0);
    CHECK_EQ(p.to_string(), std::string("3x^2 - 2x + 1"));
    CHECK_EQ(p.to_unicode(), std::string("3x\xC2\xB2 - 2x + 1"));
    CHECK_EQ(Polynomial::parse("3x^2 - 2x + 1"), p);
    CHECK_EQ(Polynomial::parse("1 - 2x + 3x^2"), p);
    CHECK_EQ(Polynomial::parse("3*x**2-2*x+1"), p);
    CHECK_EQ(Polynomial::parse("3x\xC2\xB2 \xE2\x88\x92 2x + 1"), p);
    CHECK_EQ(Polynomial::parse("x^2 + x^2"), Polynomial({0, 0, 2}));
    CHECK_EQ(Polynomial::parse("-x"), Polynomial({0, -1}));
    CHECK_EQ(Polynomial::parse("x"), Polynomial({0, 1}));
    CHECK_EQ(Polynomial::parse("7"), Polynomial({7}));
    CHECK_EQ(Polynomial::parse("1/2x + 3/4"), Polynomial({0.75, 0.5}));
    CHECK_EQ(Polynomial::parse("2.5e1x^3"), Polynomial({0, 0, 0, 25}));
    CHECK_EQ(Polynomial::parse("t^2 - 1", 't'), Polynomial({-1, 0, 1}));
    CHECK_THROWS(Polynomial::parse(""), std::invalid_argument);
    CHECK_THROWS(Polynomial::parse("x^"), std::invalid_argument);
    CHECK_THROWS(Polynomial::parse("x^-1"), std::invalid_argument);
    CHECK_THROWS(Polynomial::parse("2y"), std::invalid_argument);
    CHECK_THROWS(Polynomial::parse("x +"), std::invalid_argument);
    CHECK_THROWS(Polynomial::parse("x^99999999"), std::invalid_argument);
    CHECK_EQ(Polynomial().degree(), -1);
    CHECK(Polynomial().is_zero());
    CHECK_EQ(Polynomial().to_string(), std::string("0"));
    CHECK_EQ(Polynomial({0, 0, 0}).degree(), -1);           // trailing zeros trimmed
    CHECK_EQ(Polynomial({-1, 0, 1}).to_string(), std::string("x^2 - 1"));
    CHECK_EQ(Polynomial({0, -1}).to_string(), std::string("-x"));
    CHECK_EQ(Polynomial({2, 0, 0, -1}).to_string(), std::string("-x^3 + 2"));
    CHECK_EQ(Polynomial::monomial(3, 2.0).to_string(), std::string("2x^3"));
    CHECK_EQ(Polynomial::from_roots({1, 2}).to_string(), std::string("x^2 - 3x + 2"));
    Polynomial q;
    q.set(4, 1.0);
    CHECK_EQ(q.degree(), 4);
    q.set(4, 0.0);
    CHECK(q.is_zero());
}

TEST_CASE("polynomial: evaluation and calculus") {
    Polynomial p = Polynomial::parse("x^3 - 2x + 1");
    CHECK_EQ(p.evaluate(2.0), 5.0);
    CHECK_EQ(p(0.0), 1.0);
    const auto pd = p.evaluate_with_derivative(2.0);
    CHECK_EQ(pd.first, 5.0);
    CHECK_EQ(pd.second, 10.0);
    CHECK_EQ(p.derivative().to_string(), std::string("3x^2 - 2"));
    CHECK_EQ(p.derivative().derivative().derivative().to_string(), std::string("6"));
    CHECK_EQ(p.derivative().derivative().derivative().derivative().degree(), -1);
    CHECK_EQ(p.integral().to_string(), std::string("0.25x^4 - x^2 + x"));
    CHECK_EQ(p.integral(3.0)[0], 3.0);
    CHECK_CLOSE(p.definite_integral(0.0, 2.0), 4.0 - 4.0 + 2.0, 1e-15);
    const std::complex<double> z(1.0, 1.0);
    const auto pz = p.evaluate(z);  // (1+i)^3 - 2(1+i) + 1 = (-2+2i) - 2 - 2i + 1 = -3 + 0i
    CHECK_CLOSE_ABS(pz.real(), -3.0, 1e-14);
    CHECK_CLOSE_ABS(pz.imag(), 0.0, 1e-14);
    CHECK_EQ(p.shifted(1.0).evaluate(1.0), p.evaluate(2.0));
    CHECK_EQ(p.compose(Polynomial({0, 2})).evaluate(1.0), p.evaluate(2.0));
    CHECK_EQ(Polynomial({1, 1}).pow(3).to_string(), std::string("x^3 + 3x^2 + 3x + 1"));
    CHECK_EQ(Polynomial({2, 4}).normalized().to_string(), std::string("x + 0.5"));
}

TEST_CASE("polynomial: arithmetic, division, gcd") {
    Polynomial a = Polynomial::parse("x^2 - 1");
    Polynomial b = Polynomial::parse("x - 1");
    CHECK_EQ((a + b).to_string(), std::string("x^2 + x - 2"));
    CHECK_EQ((a - b).to_string(), std::string("x^2 - x"));
    CHECK_EQ((a * b).to_string(), std::string("x^3 - x^2 - x + 1"));
    CHECK_EQ((a * 2.0).to_string(), std::string("2x^2 - 2"));
    CHECK_EQ((2.0 * a)[2], 2.0);
    CHECK_EQ((a / 2.0)[0], -0.5);
    CHECK_EQ((a / b).to_string(), std::string("x + 1"));
    CHECK(((a % b)).is_zero());
    const auto dm = Polynomial::divmod(Polynomial::parse("x^3 + 2x + 5"), Polynomial::parse("x^2 + 1"));
    CHECK_EQ(dm.first.to_string(), std::string("x"));
    CHECK_EQ(dm.second.to_string(), std::string("x + 5"));
    CHECK_THROWS(a / Polynomial(), std::invalid_argument);
    CHECK_THROWS(a / 0.0, std::invalid_argument);
    CHECK_EQ(Polynomial::gcd(Polynomial::from_roots({1, 2}), Polynomial::from_roots({1, 3})).to_string(), std::string("x - 1"));
    CHECK_EQ(Polynomial::gcd(Polynomial::from_roots({1, 2, 3}), Polynomial::from_roots({2, 3, 4})).to_string(), std::string("x^2 - 5x + 6"));
    CHECK_EQ(Polynomial::gcd(a, Polynomial::parse("x + 7")).to_string(), std::string("1"));
    CHECK_EQ(a.deflate(1.0).to_string(), std::string("x + 1"));
    CHECK_EQ(Polynomial::parse("x^3 - 6x^2 + 11x - 6").deflate(1.0).to_string(), std::string("x^2 - 5x + 6"));
    Polynomial c = a;
    c += b; c -= b; c *= Polynomial({1}); c *= 1.0;
    CHECK_EQ(c, a);
    CHECK(a != b);
    CHECK_EQ((-a).to_string(), std::string("-x^2 + 1"));
}

TEST_CASE("polynomial: roots") {
    // distinct real roots
    const auto r1 = Polynomial::parse("x^3 - 6x^2 + 11x - 6").roots();
    CHECK(r1.converged);
    CHECK_EQ(r1.real_roots.size(), 3u);
    CHECK_CLOSE(r1.real_roots[0], 1.0, 1e-10);
    CHECK_CLOSE(r1.real_roots[1], 2.0, 1e-10);
    CHECK_CLOSE(r1.real_roots[2], 3.0, 1e-10);
    // complex pair
    const auto r2 = Polynomial::parse("x^2 + 1").roots();
    CHECK_EQ(r2.roots.size(), 2u);
    CHECK_EQ(r2.real_roots.size(), 0u);
    CHECK_CLOSE_ABS(r2.roots[0].imag(), -1.0, 1e-14);
    CHECK_CLOSE_ABS(r2.roots[1].imag(), 1.0, 1e-14);
    // quadratic with cancellation-prone coefficients
    const auto r3 = Polynomial({1e-8, -1.0, 1.0}).roots();     // x^2 - x + 1e-8
    CHECK_CLOSE(r3.real_roots[0], 1.0000000100000002e-08, 1e-9);
    CHECK_CLOSE(r3.real_roots[1], 0.99999998999999994, 1e-14);
    // roots on the unit circle
    const auto r4 = Polynomial::parse("x^5 - 1").roots();
    CHECK(r4.converged);
    CHECK_EQ(r4.roots.size(), 5u);
    for (const auto& z : r4.roots) CHECK_CLOSE(std::abs(z), 1.0, 1e-10);
    CHECK_EQ(r4.real_roots.size(), 1u);
    CHECK_CLOSE(r4.real_roots[0], 1.0, 1e-12);
    // zero roots factored out
    const auto r5 = Polynomial::parse("x^3 - x").roots();
    CHECK_EQ(r5.real_roots.size(), 3u);
    CHECK_CLOSE_ABS(r5.real_roots[0], -1.0, 1e-12);
    CHECK_CLOSE_ABS(r5.real_roots[1], 0.0, 1e-12);
    CHECK_CLOSE_ABS(r5.real_roots[2], 1.0, 1e-12);
    // repeated root converges (more slowly) to the right value
    const auto r6 = Polynomial::from_roots({2, 2, 2}).roots(1e-14, 2000);
    for (const auto& z : r6.roots) CHECK_CLOSE_ABS(z.real(), 2.0, 1e-4);
    // linear and constant
    CHECK_EQ(Polynomial::parse("2x - 4").real_roots()[0], 2.0);
    CHECK_EQ(Polynomial::parse("5").roots().roots.size(), 0u);
    CHECK_EQ(Polynomial().roots().roots.size(), 0u);
    // degree 10 with integer roots
    const auto r7 = Polynomial::from_roots({-4, -3, -2, -1, 0, 1, 2, 3, 4, 5}).roots();
    CHECK_EQ(r7.real_roots.size(), 10u);
    for (std::size_t i = 0; i < 10; ++i) CHECK_CLOSE_ABS(r7.real_roots[i], static_cast<double>(i) - 4.0, 1e-7);
}
