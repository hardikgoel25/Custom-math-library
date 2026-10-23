// tests/test_expression.cpp
#include "cml_test.hpp"
#include "cml/expression.hpp"
#include "cml/core.hpp"

#include <cmath>
#include <string>

using cml::Expression;
using cml::ExpressionError;

namespace {
double ev(const std::string& s, double x = 0.0) { return Expression::parse(s).evaluate(x); }
}

TEST_CASE("expression: arithmetic and precedence") {
    CHECK_EQ(ev("1 + 2 * 3"), 7.0);
    CHECK_EQ(ev("(1 + 2) * 3"), 9.0);
    CHECK_EQ(ev("2 ^ 3 ^ 2"), 512.0);          // right associative
    CHECK_EQ(ev("-2 ^ 2"), -4.0);              // unary minus binds looser than ^
    CHECK_EQ(ev("(-2) ^ 2"), 4.0);
    CHECK_EQ(ev("2 ^ -1"), 0.5);
    CHECK_EQ(ev("10 / 4"), 2.5);
    CHECK_EQ(ev("10 % 4"), 2.0);
    CHECK_EQ(ev("-10 % 4"), -2.0);
    CHECK_EQ(ev("5!"), 120.0);
    CHECK_EQ(ev("3! + 1"), 7.0);
    CHECK_EQ(ev("2 ^ 3!"), 64.0);
    CHECK_EQ(ev("2 ** 10"), 1024.0);
    CHECK_EQ(ev("1e3 + 2.5e-1"), 1000.25);
    CHECK_EQ(ev(".5 + .25"), 0.75);
    CHECK_EQ(ev("--3"), 3.0);
    CHECK_EQ(ev("+3"), 3.0);
    CHECK_EQ(ev("1 - 2 - 3"), -4.0);            // left associative
    CHECK_EQ(ev("2 * 3 / 4"), 1.5);
    CHECK_EQ(ev("[1 + 2] * {3}"), 9.0);
}

TEST_CASE("expression: implicit multiplication and variables") {
    CHECK_EQ(ev("2x", 3.0), 6.0);
    CHECK_EQ(ev("2x^2", 3.0), 18.0);           // 2*(x^2)
    CHECK_EQ(ev("3(x + 1)", 2.0), 9.0);
    CHECK_EQ(ev("(x+1)(x-1)", 3.0), 8.0);
    CHECK_EQ(ev("2 pi") , 2.0 * cml::constants::pi);
    CHECK_EQ(ev("x x", 4.0), 16.0);
    CHECK_EQ(ev("2sin(x)", cml::constants::half_pi), 2.0);
    CHECK_EQ(ev("1/2x", 4.0), 2.0);            // (1/2)*x, same precedence as *
    Expression e = Expression::parse("a*x^2 + b*x + c");
    Expression::Variables v{{"a", 1}, {"b", -3}, {"c", 2}, {"x", 2}};
    CHECK_EQ(e.evaluate(v), 0.0);
    const auto names = e.variables();
    CHECK_EQ(names.size(), 4u);
    CHECK_EQ(names[0], std::string("a"));
    CHECK_EQ(names[3], std::string("x"));
    CHECK(e.depends_on("b"));
    CHECK(!e.depends_on("y"));
    CHECK_THROWS(e.evaluate(2.0), ExpressionError);   // a, b, c unbound
    CHECK_EQ(Expression::parse("x_1 + x2").variables().size(), 2u);
    CHECK_EQ(Expression::parse("x*y").evaluate(2.0, 3.0), 6.0);
    CHECK_EQ(Expression::parse("t*y").evaluate(Expression::Variables{{"t", 2}, {"y", 3}}), 6.0);
}

TEST_CASE("expression: functions and constants") {
    CHECK_CLOSE(ev("sin(pi/2)"), 1.0, 1e-15);
    CHECK_CLOSE(ev("cos(0)"), 1.0, 1e-15);
    CHECK_CLOSE(ev("e^2"), std::exp(2.0), 2e-15);
    CHECK_CLOSE(ev("exp(1)"), cml::constants::e, 2e-15);
    CHECK_CLOSE(ev("ln(e)"), 1.0, 2e-15);
    CHECK_CLOSE(ev("log(100)"), std::log(100.0), 2e-15);    // log is natural log
    CHECK_CLOSE(ev("log10(1000)"), 3.0, 1e-15);
    CHECK_CLOSE(ev("log(2, 8)"), 3.0, 4e-15);               // two-argument log is log base b
    CHECK_CLOSE(ev("log2(8)"), 3.0, 1e-15);
    CHECK_CLOSE(ev("sqrt(2)^2"), 2.0, 4e-16);
    CHECK_EQ(ev("abs(-3)"), 3.0);
    CHECK_EQ(ev("max(1, 2)"), 2.0);
    CHECK_EQ(ev("min(1, 2)"), 1.0);
    CHECK_EQ(ev("floor(2.7)"), 2.0);
    CHECK_EQ(ev("ceil(2.1)"), 3.0);
    CHECK_EQ(ev("round(2.5)"), 3.0);
    CHECK_EQ(ev("sign(-2)"), -1.0);
    CHECK_EQ(ev("fact(5)"), 120.0);
    CHECK_EQ(ev("ncr(5, 2)"), 10.0);
    CHECK_EQ(ev("npr(5, 2)"), 20.0);
    CHECK_EQ(ev("gcd(12, 18)"), 6.0);
    CHECK_EQ(ev("lcm(4, 6)"), 12.0);
    CHECK_EQ(ev("isprime(97)"), 1.0);
    CHECK_EQ(ev("fib(10)"), 55.0);
    CHECK_EQ(ev("mod(7, 3)"), 1.0);
    CHECK_EQ(ev("hypot(3, 4)"), 5.0);
    CHECK_CLOSE(ev("atan2(1, 1)"), cml::constants::quarter_pi, 1e-15);
    CHECK_CLOSE(ev("gamma(5)"), 24.0, 1e-15);
    CHECK_CLOSE(ev("erf(0)"), 0.0, 1e-15);
    CHECK_CLOSE(ev("root(27, 3)"), 3.0, 1e-15);
    CHECK_CLOSE(ev("deg(pi)"), 180.0, 1e-15);
    CHECK_CLOSE(ev("rad(180)"), cml::constants::pi, 1e-15);
    CHECK_CLOSE(ev("tau"), cml::constants::tau, 0.0);
    CHECK_CLOSE(ev("phi"), cml::constants::phi, 0.0);
    CHECK(cml::is_inf(ev("inf")));
    CHECK(cml::is_nan(ev("nan")));
    CHECK(cml::is_inf(ev("1/0")));
    CHECK(cml::is_nan(ev("sqrt(-1)")));
    CHECK_CLOSE(ev("sin x", 1.0), std::sin(1.0), 1e-15);     // function application without parens
    CHECK_CLOSE(ev("sin 2x", 1.0), std::sin(2.0), 1e-15);
    CHECK_CLOSE(ev("\xCF\x80"), cml::constants::pi, 0.0);   // unicode pi
    CHECK_EQ(ev("2 \xC3\x97 3"), 6.0);                     // unicode multiplication sign
    CHECK_EQ(ev("6 \xC3\xB7 3"), 2.0);
    CHECK_EQ(ev("5 \xE2\x88\x92 2"), 3.0);                 // unicode minus
}

TEST_CASE("expression: parse errors carry positions") {
    try { Expression::parse("2 + "); CHECK(false); }
    catch (const ExpressionError& e) { CHECK_EQ(e.position(), 4u); }
    try { Expression::parse("sin(1"); CHECK(false); }
    catch (const ExpressionError& e) { CHECK_EQ(e.position(), 5u); }
    try { Expression::parse("2 $ 3"); CHECK(false); }
    catch (const ExpressionError& e) { CHECK_EQ(e.position(), 2u); }
    try { Expression::parse("max(1)"); CHECK(false); }
    catch (const ExpressionError& e) { CHECK_EQ(e.position(), 0u); CHECK(std::string(e.what()).find("expects 2") != std::string::npos); }
    CHECK_THROWS(Expression::parse(""), ExpressionError);
    CHECK_THROWS(Expression::parse("   "), ExpressionError);
    CHECK_THROWS(Expression::parse(")"), ExpressionError);
    CHECK_THROWS(Expression::parse("(1"), ExpressionError);
    CHECK_THROWS(Expression::parse("1 2 +"), ExpressionError);
    CHECK_THROWS(Expression::parse("sin"), ExpressionError);
    CHECK_THROWS(Expression::parse("* 2"), ExpressionError);
    CHECK_THROWS(Expression::parse("1,2"), ExpressionError);
    // resource limits
    std::string deep(300, '(');
    deep += "1";
    deep += std::string(300, ')');
    CHECK_THROWS(Expression::parse(deep), ExpressionError);
    Expression::Limits lim;
    lim.max_length = 5;
    CHECK_THROWS(Expression::parse("1+2+3+4", lim), ExpressionError);
    lim.max_length = 100000;
    lim.max_nodes = 10;
    CHECK_THROWS(Expression::parse("1+2+3+4+5+6+7+8+9+10+11", lim), ExpressionError);
    // Variables shadowed by function names are a parse error, not silent multiplication
    CHECK_THROWS(Expression::parse("foo(1)").evaluate(1.0), ExpressionError); // unbound variable foo
}

TEST_CASE("expression: symbolic derivative matches numeric") {
    const char* cases[] = {
        "x^3", "sin(x)*exp(x)", "ln(x^2 + 1)", "sqrt(x)", "1/x", "x^x", "2^x", "tan(x)", "atan(x)",
        "sinh(x)*cosh(x)", "exp(-x^2)", "(x+1)/(x-1)", "abs(x)", "x*sin(x)/cos(x)", "log10(x)", "cbrt(x)",
        "asin(x/3)", "acos(x/3)", "erf(x)", "gamma(x)", "hypot(x, 2)", "atan2(x, 1)", "pow(x, 2.5)",
        "tanh(x)", "asinh(x)", "acosh(x + 1)", "atanh(x/3)", "x^2 % 3 + 0*x", "root(x, 3)", "log(3, x)",
    };
    for (const char* src : cases) {
        Expression f = Expression::parse(src);
        Expression df;
        try {
            df = f.derivative("x");
        } catch (const ExpressionError&) {
            continue; // mod is explicitly non-differentiable
        }
        for (double x : {0.7, 1.3, 2.1}) {
            const double h = 1e-5;
            const double numeric = (f.evaluate(x + h) - f.evaluate(x - h)) / (2 * h);
            const double symbolic = df.evaluate(x);
            if (!std::isfinite(numeric)) continue;
            ++cmltest::state().checks;
            if (!cmltest::close(symbolic, numeric, 1e-6, 1e-7))
                cmltest::report_failure(__FILE__, __LINE__, std::string("d/dx ") + src + " at " + cmltest::show(x) +
                                                                ": symbolic " + cmltest::show(symbolic) + " vs numeric " +
                                                                cmltest::show(numeric) + "  [" + df.to_string() + "]");
        }
    }
    CHECK_EQ(Expression::parse("x^3").derivative().to_string(), std::string("3*x^2"));
    CHECK_EQ(Expression::parse("5").derivative().to_string(), std::string("0"));
    CHECK_EQ(Expression::parse("y").derivative("x").to_string(), std::string("0"));
    CHECK_EQ(Expression::parse("sin(x)").derivative().to_string(), std::string("cos(x)"));
    CHECK_EQ(Expression::parse("exp(2x)").derivative().to_string(), std::string("2*exp(2*x)"));
    CHECK_THROWS(Expression::parse("floor(x)!").derivative(), ExpressionError);
    CHECK_THROWS(Expression::parse("mod(x, 2)").derivative(), ExpressionError);
    // second derivative
    Expression d2 = Expression::parse("x^4").derivative().derivative();
    CHECK_EQ(d2.evaluate(2.0), 48.0);
}

TEST_CASE("expression: printing, simplification, substitution, compilation") {
    CHECK_EQ(Expression::parse("1 + 2 * 3").simplified().to_string(), std::string("7"));
    CHECK_EQ(Expression::parse("x * 1 + 0").simplified().to_string(), std::string("x"));
    CHECK_EQ(Expression::parse("0 * x + x^1").simplified().to_string(), std::string("x"));
    CHECK_EQ(Expression::parse("-(-x)").simplified().to_string(), std::string("x"));
    CHECK_EQ(Expression::parse("x + -2").simplified().to_string(), std::string("x - 2"));
    CHECK_EQ(Expression::parse("(x + 1) * (x - 1)").to_string(), std::string("(x + 1)*(x - 1)"));
    CHECK_EQ(Expression::parse("2^(x+1)").to_string(), std::string("2^(x + 1)"));
    CHECK_EQ(Expression::parse("(2^x)^2").to_string(), std::string("(2^x)^2"));
    CHECK_EQ(Expression::parse("-x^2").to_string(), std::string("-x^2"));
    CHECK_EQ(Expression::parse("(-x)^2").to_string(), std::string("(-x)^2"));
    CHECK_EQ(Expression::parse("a - (b - c)").to_string(), std::string("a - (b - c)"));
    CHECK_EQ(Expression::parse("a / (b * c)").to_string(), std::string("a/(b*c)"));
    // Round trip: printing then re-parsing evaluates identically.
    for (const char* src : {"sin(x)^2 + cos(x)^2", "x^-2", "-x^-2", "2x/(3x - 1)", "x!^2", "pow(x, 2)/x"}) {
        Expression e = Expression::parse(src);
        Expression again = Expression::parse(e.to_string());
        CHECK_CLOSE(again.evaluate(1.7), e.evaluate(1.7), 1e-15);
    }
    Expression sub = Expression::parse("a*x + b").substitute("a", 2.0).substitute("b", 1.0);
    CHECK_EQ(sub.evaluate(3.0), 7.0);
    CHECK_EQ(sub.variables().size(), 1u);
    Expression comp = Expression::parse("x^2").substitute("x", Expression::parse("sin(t)"));
    CHECK_CLOSE(comp.evaluate(Expression::Variables{{"t", 0.5}}), std::sin(0.5) * std::sin(0.5), 1e-15);
    // compiled fast path agrees with the tree walker
    Expression f = Expression::parse("3x^2 - 2x + sin(x)*exp(-x) + x!");
    auto fn = f.as_function("x");
    for (double x : {0.5, 1.0, 2.5, 4.0}) CHECK_CLOSE(fn(x), f.evaluate(x), 1e-15);
    auto fn2 = Expression::parse("x^2 + y^2").as_function2("x", "y");
    CHECK_EQ(fn2(3.0, 4.0), 25.0);
    auto fnn = Expression::parse("a + 2b + 3c").as_function_n({"a", "b", "c"});
    CHECK_EQ(fnn({1.0, 1.0, 1.0}), 6.0);
    CHECK_THROWS(Expression::parse("x + y").compile({"x"}), ExpressionError);
    CHECK(Expression::is_function("sin"));
    CHECK(!Expression::is_function("x"));
    CHECK(Expression::is_constant_name("pi"));
    CHECK(Expression::function_names().size() > 50);
    CHECK(Expression::parse("3").is_constant());
    CHECK_EQ(Expression::parse("3").constant_value(), 3.0);
    CHECK_EQ(Expression::parse("1+2+3").node_count(), 5u);
}
