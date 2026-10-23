// cml/expression.hpp — parse, evaluate, differentiate and print math expressions.
//
//   Expression f = Expression::parse("sin(x)^2 + 3x - 1");
//   double y = f.evaluate(0.5);
//   Expression df = f.derivative("x");      // symbolic
//   auto fn = f.as_function("x");           // fast std::function<double(double)>
//
// Grammar highlights: + - * / ^ % (mod), unary minus, postfix ! (factorial),
// implicit multiplication (2x, 3sin(x), (x+1)(x-1)), ~50 built-in functions,
// constants pi, e, tau, phi, inf, nan.  Free variables are any identifier
// that is not a function or constant.  All evaluation uses cml::core, so the
// whole expression stack runs on the from-scratch implementations.
#ifndef CML_EXPRESSION_HPP
#define CML_EXPRESSION_HPP

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace cml {

class ExpressionError : public std::runtime_error {
public:
    ExpressionError(const std::string& msg, std::size_t position)
        : std::runtime_error(msg), position_(position) {}
    std::size_t position() const { return position_; }
private:
    std::size_t position_;
};

struct ExprNode; // opaque AST node

class Expression {
public:
    using Variables = std::map<std::string, double>;

    struct Limits {
        std::size_t max_length = 20000;  // characters
        std::size_t max_depth  = 256;    // nesting
        std::size_t max_nodes  = 50000;  // AST size
    };

    Expression();  // the constant 0
    static Expression parse(const std::string& text);
    static Expression parse(const std::string& text, const Limits& limits);
    static Expression constant(double value);
    static Expression variable(const std::string& name);

    bool is_constant() const;
    double constant_value() const;        // valid if is_constant()
    std::string to_string() const;
    const std::string& source() const { return source_; }

    std::vector<std::string> variables() const; // sorted, unique
    bool depends_on(const std::string& var) const;
    std::size_t node_count() const;

    double evaluate(const Variables& vars) const;
    double evaluate() const;                         // no variables
    double evaluate(double x) const;                 // binds "x"
    double evaluate(double x, double y) const;       // binds "x" and "y"

    Expression derivative(const std::string& var = "x") const; // throws ExpressionError if not differentiable
    Expression simplified() const;
    Expression substitute(const std::string& var, double value) const;
    Expression substitute(const std::string& var, const Expression& with) const;

    // Compiled evaluation: resolves variable slots once, then evaluates with a
    // flat array of values in the order given.
    class Compiled {
    public:
        double operator()(const double* values, std::size_t count) const;
        double operator()(double x) const { return (*this)(&x, 1); }
        double operator()(double x, double y) const { const double v[2] = {x, y}; return (*this)(v, 2); }
        const std::vector<std::string>& names() const { return names_; }
    private:
        friend class Expression;
        struct Instr { int op; int fn; int slot; double value; };
        std::vector<Instr> code_;
        std::vector<std::string> names_;
    };
    Compiled compile(const std::vector<std::string>& names) const;
    std::function<double(double)> as_function(const std::string& var = "x") const;
    std::function<double(double, double)> as_function2(const std::string& v1 = "x", const std::string& v2 = "y") const;
    std::function<double(const std::vector<double>&)> as_function_n(const std::vector<std::string>& names) const;

    static const std::vector<std::string>& function_names();
    static const std::vector<std::string>& constant_names();
    static bool is_function(const std::string& name);
    static bool is_constant_name(const std::string& name);

private:
    explicit Expression(std::shared_ptr<const ExprNode> root, std::string source = "");
    std::shared_ptr<const ExprNode> root_;
    std::string source_;
};

} // namespace cml

#endif // CML_EXPRESSION_HPP
