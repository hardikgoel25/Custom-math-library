// cml/expression.cpp — Pratt parser, evaluator, symbolic differentiation.
#include "cml/expression.hpp"
#include "cml/core.hpp"
#include "cml/special.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <set>

namespace cml {

// ---------------------------------------------------------------------------
// AST
// ---------------------------------------------------------------------------
enum class NodeKind { Number, Variable, Neg, Add, Sub, Mul, Div, Pow, Mod, Call };

struct ExprNode {
    NodeKind kind;
    double value = 0.0;               // Number
    std::string name;                 // Variable or function name
    int fn = -1;                      // function index (Call)
    std::vector<std::shared_ptr<const ExprNode>> args; // operands

    explicit ExprNode(NodeKind k) : kind(k) {}
};

using NodePtr = std::shared_ptr<const ExprNode>;

namespace {

NodePtr make_number(double v) {
    auto n = std::make_shared<ExprNode>(NodeKind::Number);
    n->value = v;
    return n;
}
NodePtr make_variable(const std::string& name) {
    auto n = std::make_shared<ExprNode>(NodeKind::Variable);
    n->name = name;
    return n;
}
NodePtr make_unary(NodeKind k, NodePtr a) {
    auto n = std::make_shared<ExprNode>(k);
    n->args.push_back(std::move(a));
    return n;
}
NodePtr make_binary(NodeKind k, NodePtr a, NodePtr b) {
    auto n = std::make_shared<ExprNode>(k);
    n->args.push_back(std::move(a));
    n->args.push_back(std::move(b));
    return n;
}

// ---------------------------------------------------------------------------
// Built-in functions
// ---------------------------------------------------------------------------
struct FnInfo {
    const char* name;
    int arity;
    double (*f1)(double);
    double (*f2)(double, double);
};

double fn_fact(double x) { return factorial(x); }
double fn_deg(double x) { return rad2deg(x); }
double fn_rad(double x) { return deg2rad(x); }
double fn_sign(double x) { return static_cast<double>(sign(x)); }
double fn_sq(double x) { return x * x; }
double fn_min(double a, double b) { return fmin(a, b); }
double fn_max(double a, double b) { return fmax(a, b); }
double fn_mod(double a, double b) { return fmod(a, b); }
double fn_logb(double b, double x) { return log_base(b, x); }
double fn_root(double x, double n) { return is_integer(n) ? nth_root(x, static_cast<int>(n)) : pow(x, 1.0 / n); }
double fn_ncr(double n, double k) { return binomial(n, k); }
double fn_npr(double n, double k) { return permutations(n, k); }
double fn_gcd(double a, double b) {
    if (!is_integer(a) || !is_integer(b) || a < 0 || b < 0 || a > 9e15 || b > 9e15) return nan();
    return static_cast<double>(gcd(static_cast<std::uint64_t>(a), static_cast<std::uint64_t>(b)));
}
double fn_lcm(double a, double b) {
    if (!is_integer(a) || !is_integer(b) || a < 0 || b < 0 || a > 9e15 || b > 9e15) return nan();
    return static_cast<double>(lcm(static_cast<std::uint64_t>(a), static_cast<std::uint64_t>(b)));
}
double fn_isprime(double n) {
    if (!is_integer(n) || n < 0 || n > 1.8e19) return nan();
    return is_prime(static_cast<std::uint64_t>(n)) ? 1.0 : 0.0;
}
double fn_fib(double n) {
    if (!is_integer(n) || n < 0 || n > 93) return nan();
    return static_cast<double>(fibonacci(static_cast<unsigned>(n)));
}
double fn_normcdf(double x) { return normal_cdf(x); }
double fn_normpdf(double x) { return normal_pdf(x); }

const FnInfo FUNCTIONS[] = {
    {"sin", 1, sin, nullptr},       {"cos", 1, cos, nullptr},       {"tan", 1, tan, nullptr},
    {"csc", 1, csc, nullptr},       {"sec", 1, sec, nullptr},       {"cot", 1, cot, nullptr},
    {"asin", 1, asin, nullptr},     {"acos", 1, acos, nullptr},     {"atan", 1, atan, nullptr},
    {"acsc", 1, acsc, nullptr},     {"asec", 1, asec, nullptr},     {"acot", 1, acot, nullptr},
    {"sinh", 1, sinh, nullptr},     {"cosh", 1, cosh, nullptr},     {"tanh", 1, tanh, nullptr},
    {"csch", 1, csch, nullptr},     {"sech", 1, sech, nullptr},     {"coth", 1, coth, nullptr},
    {"asinh", 1, asinh, nullptr},   {"acosh", 1, acosh, nullptr},   {"atanh", 1, atanh, nullptr},
    {"acsch", 1, acsch, nullptr},   {"asech", 1, asech, nullptr},   {"acoth", 1, acoth, nullptr},
    {"exp", 1, exp, nullptr},       {"exp2", 1, exp2, nullptr},     {"expm1", 1, expm1, nullptr},
    {"ln", 1, ln, nullptr},         {"log", 1, ln, nullptr},        {"log2", 1, log2, nullptr},
    {"log10", 1, log10, nullptr},   {"log1p", 1, log1p, nullptr},   {"sqrt", 1, sqrt, nullptr},
    {"cbrt", 1, cbrt, nullptr},     {"abs", 1, abs, nullptr},       {"floor", 1, floor, nullptr},
    {"ceil", 1, ceil, nullptr},     {"round", 1, round, nullptr},   {"trunc", 1, trunc, nullptr},
    {"sign", 1, fn_sign, nullptr},  {"sq", 1, fn_sq, nullptr},      {"gamma", 1, gamma, nullptr},
    {"lgamma", 1, lgamma, nullptr}, {"digamma", 1, digamma, nullptr}, {"erf", 1, erf, nullptr},
    {"erfc", 1, erfc, nullptr},     {"fact", 1, fn_fact, nullptr},  {"deg", 1, fn_deg, nullptr},
    {"rad", 1, fn_rad, nullptr},    {"isprime", 1, fn_isprime, nullptr}, {"fib", 1, fn_fib, nullptr},
    {"normcdf", 1, fn_normcdf, nullptr}, {"normpdf", 1, fn_normpdf, nullptr},
    {"pow", 2, nullptr, pow},       {"atan2", 2, nullptr, atan2},   {"hypot", 2, nullptr, hypot},
    {"min", 2, nullptr, fn_min},    {"max", 2, nullptr, fn_max},    {"mod", 2, nullptr, fn_mod},
    {"logb", 2, nullptr, fn_logb},  {"root", 2, nullptr, fn_root},  {"beta", 2, nullptr, beta},
    {"ncr", 2, nullptr, fn_ncr},    {"npr", 2, nullptr, fn_npr},    {"gcd", 2, nullptr, fn_gcd},
    {"lcm", 2, nullptr, fn_lcm},
};
const int FUNCTION_COUNT = static_cast<int>(sizeof(FUNCTIONS) / sizeof(FUNCTIONS[0]));

int find_function(const std::string& name) {
    for (int i = 0; i < FUNCTION_COUNT; ++i)
        if (name == FUNCTIONS[i].name) return i;
    return -1;
}
int fn_index(const char* name) { return find_function(name); }

struct ConstInfo { const char* name; double value; };
const ConstInfo CONSTANTS[] = {
    {"pi", constants::pi},   {"e", constants::e},     {"tau", constants::tau},
    {"phi", constants::phi}, {"inf", 0.0},            {"nan", 0.0},
    {"eulergamma", constants::euler_gamma},
};
const int CONSTANT_COUNT = static_cast<int>(sizeof(CONSTANTS) / sizeof(CONSTANTS[0]));

bool lookup_constant(const std::string& name, double* out) {
    for (int i = 0; i < CONSTANT_COUNT; ++i) {
        if (name == CONSTANTS[i].name) {
            if (name == "inf") *out = inf();
            else if (name == "nan") *out = nan();
            else *out = CONSTANTS[i].value;
            return true;
        }
    }
    return false;
}

NodePtr make_call(int fn, std::vector<NodePtr> args) {
    auto n = std::make_shared<ExprNode>(NodeKind::Call);
    n->fn = fn;
    n->name = FUNCTIONS[fn].name;
    n->args = std::move(args);
    return n;
}
NodePtr call1(const char* name, NodePtr a) { return make_call(fn_index(name), {std::move(a)}); }

// ---------------------------------------------------------------------------
// Tokenizer
// ---------------------------------------------------------------------------
enum class Tok { End, Number, Ident, Plus, Minus, Star, Slash, Caret, Percent, Bang, LParen, RParen, Comma };

struct Token {
    Tok kind;
    double number = 0.0;
    std::string text;
    std::size_t pos = 0;
};

class Lexer {
public:
    explicit Lexer(const std::string& s) : s_(s) {}
    Token next() {
        while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) ++pos_;
        Token t;
        t.pos = pos_;
        if (pos_ >= s_.size()) { t.kind = Tok::End; return t; }
        const char c = s_[pos_];
        if ((c >= '0' && c <= '9') || (c == '.' && pos_ + 1 < s_.size() && s_[pos_ + 1] >= '0' && s_[pos_ + 1] <= '9')) {
            const std::size_t start = pos_;
            while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
            if (pos_ < s_.size() && s_[pos_] == '.') {
                ++pos_;
                while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
            }
            if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
                std::size_t p = pos_ + 1;
                if (p < s_.size() && (s_[p] == '+' || s_[p] == '-')) ++p;
                if (p < s_.size() && s_[p] >= '0' && s_[p] <= '9') {
                    pos_ = p;
                    while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
                }
            }
            t.kind = Tok::Number;
            t.text = s_.substr(start, pos_ - start);
            t.number = std::strtod(t.text.c_str(), nullptr);
            return t;
        }
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            const std::size_t start = pos_;
            while (pos_ < s_.size() && ((s_[pos_] >= 'a' && s_[pos_] <= 'z') || (s_[pos_] >= 'A' && s_[pos_] <= 'Z') ||
                                        (s_[pos_] >= '0' && s_[pos_] <= '9') || s_[pos_] == '_'))
                ++pos_;
            t.kind = Tok::Ident;
            t.text = s_.substr(start, pos_ - start);
            return t;
        }
        ++pos_;
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 0x80) {
            // A few common UTF-8 math symbols: × (C3 97), ÷ (C3 B7), π (CF 80), ⋅ (E2 8B 85), − (E2 88 92)
            auto next_is = [&](std::size_t offset, unsigned char want) {
                return pos_ + offset < s_.size() && static_cast<unsigned char>(s_[pos_ + offset]) == want;
            };
            if (uc == 0xC3 && next_is(0, 0x97)) { ++pos_; t.kind = Tok::Star; return t; }
            if (uc == 0xC3 && next_is(0, 0xB7)) { ++pos_; t.kind = Tok::Slash; return t; }
            if (uc == 0xCF && next_is(0, 0x80)) { ++pos_; t.kind = Tok::Ident; t.text = "pi"; return t; }
            if (uc == 0xE2 && next_is(0, 0x8B) && next_is(1, 0x85)) { pos_ += 2; t.kind = Tok::Star; return t; }
            if (uc == 0xE2 && next_is(0, 0x88) && next_is(1, 0x92)) { pos_ += 2; t.kind = Tok::Minus; return t; }
            throw ExpressionError("unexpected character", t.pos);
        }
        switch (c) {
            case '+': t.kind = Tok::Plus; break;
            case '-': t.kind = Tok::Minus; break;
            case '*':
                if (pos_ < s_.size() && s_[pos_] == '*') { ++pos_; t.kind = Tok::Caret; } // ** as power
                else t.kind = Tok::Star;
                break;
            case '/': t.kind = Tok::Slash; break;
            case '^': t.kind = Tok::Caret; break;
            case '%': t.kind = Tok::Percent; break;
            case '!': t.kind = Tok::Bang; break;
            case '(': case '[': case '{': t.kind = Tok::LParen; break;
            case ')': case ']': case '}': t.kind = Tok::RParen; break;
            case ',': t.kind = Tok::Comma; break;
            default: {
                std::string msg = "unexpected character '";
                msg.push_back(c);
                msg += "'";
                throw ExpressionError(msg, t.pos);
            }
        }
        return t;
    }

private:
    const std::string& s_;
    std::size_t pos_ = 0;
};

// ---------------------------------------------------------------------------
// Pratt parser
// ---------------------------------------------------------------------------
class Parser {
public:
    Parser(const std::string& s, const Expression::Limits& limits) : lexer_(s), limits_(limits) { advance(); }

    NodePtr parse() {
        NodePtr e = parse_expr(0, 0);
        if (cur_.kind != Tok::End) {
            if (cur_.kind == Tok::RParen) throw ExpressionError("unmatched ')'", cur_.pos);
            throw ExpressionError("unexpected token", cur_.pos);
        }
        return e;
    }

private:
    Lexer lexer_;
    Token cur_;
    Expression::Limits limits_;
    std::size_t nodes_ = 0;

    void advance() { cur_ = lexer_.next(); }
    void count_node() {
        if (++nodes_ > limits_.max_nodes) throw ExpressionError("expression too large", cur_.pos);
    }

    static int infix_bp(Tok k, bool* right_assoc) {
        *right_assoc = false;
        switch (k) {
            case Tok::Plus: case Tok::Minus: return 2;
            case Tok::Star: case Tok::Slash: case Tok::Percent: return 4;
            case Tok::Caret: *right_assoc = true; return 8;
            default: return -1;
        }
    }
    static const int UNARY_BP = 6;      // -x^2 == -(x^2); -x*y == (-x)*y
    static const int POSTFIX_BP = 10;   // 2^3! == 2^(3!)
    static const int IMPLICIT_BP = 4;   // 2x == 2*x, same level as '*'

    bool starts_operand() const {
        return cur_.kind == Tok::Number || cur_.kind == Tok::Ident || cur_.kind == Tok::LParen;
    }

    NodePtr parse_expr(int min_bp, std::size_t depth) {
        if (depth > limits_.max_depth) throw ExpressionError("expression nested too deeply", cur_.pos);
        NodePtr lhs = parse_prefix(depth);
        while (true) {
            // postfix factorial
            if (cur_.kind == Tok::Bang) {
                if (POSTFIX_BP < min_bp) break;
                advance();
                count_node();
                lhs = call1("fact", lhs);
                continue;
            }
            bool right = false;
            int bp = infix_bp(cur_.kind, &right);
            NodeKind kind;
            bool implicit = false;
            if (bp < 0) {
                if (starts_operand()) { bp = IMPLICIT_BP; implicit = true; kind = NodeKind::Mul; }
                else break;
            } else {
                switch (cur_.kind) {
                    case Tok::Plus: kind = NodeKind::Add; break;
                    case Tok::Minus: kind = NodeKind::Sub; break;
                    case Tok::Star: kind = NodeKind::Mul; break;
                    case Tok::Slash: kind = NodeKind::Div; break;
                    case Tok::Percent: kind = NodeKind::Mod; break;
                    default: kind = NodeKind::Pow; break;
                }
            }
            if (bp < min_bp) break;
            if (!implicit) advance();
            NodePtr rhs = parse_expr(right ? bp : bp + 1, depth + 1);
            count_node();
            lhs = make_binary(kind, lhs, rhs);
        }
        return lhs;
    }

    NodePtr parse_prefix(std::size_t depth) {
        const Token t = cur_;
        switch (t.kind) {
            case Tok::Number: {
                advance();
                count_node();
                return make_number(t.number);
            }
            case Tok::Minus: {
                advance();
                NodePtr operand = parse_expr(UNARY_BP, depth + 1);
                count_node();
                return make_unary(NodeKind::Neg, operand);
            }
            case Tok::Plus: {
                advance();
                return parse_expr(UNARY_BP, depth + 1);
            }
            case Tok::LParen: {
                advance();
                NodePtr inner = parse_expr(0, depth + 1);
                if (cur_.kind != Tok::RParen) throw ExpressionError("expected ')'", cur_.pos);
                advance();
                return inner;
            }
            case Tok::Ident: {
                advance();
                const int fn = find_function(t.text);
                if (fn >= 0) {
                    if (cur_.kind != Tok::LParen) {
                        // Allow "sin x" style application of a one-argument function.
                        if (FUNCTIONS[fn].arity == 1 && starts_operand()) {
                            NodePtr arg = parse_expr(UNARY_BP + 1, depth + 1);
                            count_node();
                            return make_call(fn, {arg});
                        }
                        throw ExpressionError("function '" + t.text + "' requires parentheses", t.pos);
                    }
                    advance();
                    std::vector<NodePtr> args;
                    if (cur_.kind != Tok::RParen) {
                        while (true) {
                            args.push_back(parse_expr(0, depth + 1));
                            if (cur_.kind == Tok::Comma) { advance(); continue; }
                            break;
                        }
                    }
                    if (cur_.kind != Tok::RParen) throw ExpressionError("expected ')' after arguments", cur_.pos);
                    advance();
                    // "log(b, x)" with two arguments means log base b.
                    int chosen = fn;
                    if (t.text == "log" && args.size() == 2) chosen = fn_index("logb");
                    const int arity = FUNCTIONS[chosen].arity;
                    if (static_cast<int>(args.size()) != arity)
                        throw ExpressionError("function '" + t.text + "' expects " + std::to_string(arity) +
                                                  " argument(s), got " + std::to_string(args.size()),
                                              t.pos);
                    count_node();
                    return make_call(chosen, std::move(args));
                }
                double cv;
                if (lookup_constant(t.text, &cv)) {
                    count_node();
                    return make_number(cv);
                }
                count_node();
                return make_variable(t.text);
            }
            case Tok::End:
                throw ExpressionError("unexpected end of expression", t.pos);
            case Tok::RParen:
                throw ExpressionError("unexpected ')'", t.pos);
            default:
                throw ExpressionError("unexpected operator", t.pos);
        }
    }
};

// ---------------------------------------------------------------------------
// Evaluation (tree-walking; used for one-off evaluation)
// ---------------------------------------------------------------------------
double eval_node(const ExprNode& n, const Expression::Variables& vars) {
    switch (n.kind) {
        case NodeKind::Number: return n.value;
        case NodeKind::Variable: {
            auto it = vars.find(n.name);
            if (it == vars.end()) throw ExpressionError("unbound variable '" + n.name + "'", 0);
            return it->second;
        }
        case NodeKind::Neg: return -eval_node(*n.args[0], vars);
        case NodeKind::Add: return eval_node(*n.args[0], vars) + eval_node(*n.args[1], vars);
        case NodeKind::Sub: return eval_node(*n.args[0], vars) - eval_node(*n.args[1], vars);
        case NodeKind::Mul: return eval_node(*n.args[0], vars) * eval_node(*n.args[1], vars);
        case NodeKind::Div: return eval_node(*n.args[0], vars) / eval_node(*n.args[1], vars);
        case NodeKind::Pow: return pow(eval_node(*n.args[0], vars), eval_node(*n.args[1], vars));
        case NodeKind::Mod: return fmod(eval_node(*n.args[0], vars), eval_node(*n.args[1], vars));
        case NodeKind::Call: {
            const FnInfo& f = FUNCTIONS[n.fn];
            if (f.arity == 1) return f.f1(eval_node(*n.args[0], vars));
            return f.f2(eval_node(*n.args[0], vars), eval_node(*n.args[1], vars));
        }
    }
    return nan();
}

// ---------------------------------------------------------------------------
// Printing
// ---------------------------------------------------------------------------
int precedence(const ExprNode& n) {
    switch (n.kind) {
        case NodeKind::Add: case NodeKind::Sub: return 2;
        case NodeKind::Mul: case NodeKind::Div: case NodeKind::Mod: return 4;
        case NodeKind::Neg: return 6;
        case NodeKind::Pow: return 8;
        default: return 10;
    }
}

std::string format_number(double v) {
    if (is_nan(v)) return "nan";
    if (is_inf(v)) return v > 0 ? "inf" : "-inf";
    if (v == constants::pi) return "pi";
    if (v == constants::e) return "e";
    return to_string(v, 15);
}

void print_node(const ExprNode& n, std::string& out) {
    auto child = [&](const ExprNode& c, int min_prec, std::string& o) {
        const bool paren = precedence(c) < min_prec || (c.kind == NodeKind::Number && c.value < 0 && min_prec > 2);
        if (paren) o.push_back('(');
        print_node(c, o);
        if (paren) o.push_back(')');
    };
    switch (n.kind) {
        case NodeKind::Number: out += format_number(n.value); break;
        case NodeKind::Variable: out += n.name; break;
        case NodeKind::Neg:
            out.push_back('-');
            child(*n.args[0], 6, out);
            break;
        case NodeKind::Add: child(*n.args[0], 2, out); out += " + "; child(*n.args[1], 3, out); break;
        case NodeKind::Sub: child(*n.args[0], 2, out); out += " - "; child(*n.args[1], 3, out); break;
        case NodeKind::Mul: child(*n.args[0], 4, out); out += "*";   child(*n.args[1], 5, out); break;
        case NodeKind::Div: child(*n.args[0], 4, out); out += "/";   child(*n.args[1], 5, out); break;
        case NodeKind::Mod: child(*n.args[0], 4, out); out += " % "; child(*n.args[1], 5, out); break;
        case NodeKind::Pow: child(*n.args[0], 9, out); out += "^";   child(*n.args[1], 8, out); break;
        case NodeKind::Call: {
            out += n.name;
            out.push_back('(');
            for (std::size_t i = 0; i < n.args.size(); ++i) {
                if (i) out += ", ";
                print_node(*n.args[i], out);
            }
            out.push_back(')');
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Simplification (constant folding + algebraic identities)
// ---------------------------------------------------------------------------
bool is_num(const NodePtr& n, double v) { return n->kind == NodeKind::Number && n->value == v; }
bool is_number(const NodePtr& n) { return n->kind == NodeKind::Number; }

NodePtr simplify(const NodePtr& n);

NodePtr fold(NodeKind kind, const std::vector<NodePtr>& args, int fn) {
    auto node = std::make_shared<ExprNode>(kind);
    node->args = args;
    node->fn = fn;
    if (fn >= 0) node->name = FUNCTIONS[fn].name;
    Expression::Variables none;
    bool all_const = true;
    for (const auto& a : args)
        if (!is_number(a)) { all_const = false; break; }
    if (all_const) {
        const double v = eval_node(*node, none);
        // Keep exact-looking results only; avoid folding to inf/nan from e.g. 1/0 so
        // the printed expression still shows the structure.
        if (is_finite(v)) return make_number(v);
    }
    return node;
}

NodePtr simplify(const NodePtr& n) {
    switch (n->kind) {
        case NodeKind::Number:
        case NodeKind::Variable:
            return n;
        case NodeKind::Neg: {
            NodePtr a = simplify(n->args[0]);
            if (is_number(a)) return make_number(-a->value);
            if (a->kind == NodeKind::Neg) return a->args[0];
            return fold(NodeKind::Neg, {a}, -1);
        }
        case NodeKind::Add: {
            NodePtr a = simplify(n->args[0]), b = simplify(n->args[1]);
            if (is_num(a, 0.0)) return b;
            if (is_num(b, 0.0)) return a;
            if (b->kind == NodeKind::Neg) return fold(NodeKind::Sub, {a, b->args[0]}, -1);
            if (is_number(b) && b->value < 0) return fold(NodeKind::Sub, {a, make_number(-b->value)}, -1);
            return fold(NodeKind::Add, {a, b}, -1);
        }
        case NodeKind::Sub: {
            NodePtr a = simplify(n->args[0]), b = simplify(n->args[1]);
            if (is_num(b, 0.0)) return a;
            if (is_num(a, 0.0)) return simplify(make_unary(NodeKind::Neg, b));
            if (b->kind == NodeKind::Neg) return fold(NodeKind::Add, {a, b->args[0]}, -1);
            return fold(NodeKind::Sub, {a, b}, -1);
        }
        case NodeKind::Mul: {
            NodePtr a = simplify(n->args[0]), b = simplify(n->args[1]);
            if (is_num(a, 0.0) || is_num(b, 0.0)) return make_number(0.0);
            if (is_num(a, 1.0)) return b;
            if (is_num(b, 1.0)) return a;
            if (is_num(a, -1.0)) return simplify(make_unary(NodeKind::Neg, b));
            if (is_num(b, -1.0)) return simplify(make_unary(NodeKind::Neg, a));
            if (is_number(b) && !is_number(a)) std::swap(a, b); // constants first
            if (a->kind == NodeKind::Neg && b->kind == NodeKind::Neg)
                return fold(NodeKind::Mul, {a->args[0], b->args[0]}, -1);
            return fold(NodeKind::Mul, {a, b}, -1);
        }
        case NodeKind::Div: {
            NodePtr a = simplify(n->args[0]), b = simplify(n->args[1]);
            if (is_num(b, 1.0)) return a;
            if (is_num(a, 0.0) && !is_num(b, 0.0)) return make_number(0.0);
            return fold(NodeKind::Div, {a, b}, -1);
        }
        case NodeKind::Pow: {
            NodePtr a = simplify(n->args[0]), b = simplify(n->args[1]);
            if (is_num(b, 0.0)) return make_number(1.0);
            if (is_num(b, 1.0)) return a;
            if (is_num(a, 1.0)) return make_number(1.0);
            return fold(NodeKind::Pow, {a, b}, -1);
        }
        case NodeKind::Mod: {
            NodePtr a = simplify(n->args[0]), b = simplify(n->args[1]);
            return fold(NodeKind::Mod, {a, b}, -1);
        }
        case NodeKind::Call: {
            std::vector<NodePtr> args;
            for (const auto& a : n->args) args.push_back(simplify(a));
            return fold(NodeKind::Call, args, n->fn);
        }
    }
    return n;
}

// ---------------------------------------------------------------------------
// Symbolic differentiation
// ---------------------------------------------------------------------------
NodePtr num(double v) { return make_number(v); }
NodePtr add(NodePtr a, NodePtr b) { return make_binary(NodeKind::Add, std::move(a), std::move(b)); }
NodePtr sub(NodePtr a, NodePtr b) { return make_binary(NodeKind::Sub, std::move(a), std::move(b)); }
NodePtr mul(NodePtr a, NodePtr b) { return make_binary(NodeKind::Mul, std::move(a), std::move(b)); }
NodePtr divide(NodePtr a, NodePtr b) { return make_binary(NodeKind::Div, std::move(a), std::move(b)); }
NodePtr power(NodePtr a, NodePtr b) { return make_binary(NodeKind::Pow, std::move(a), std::move(b)); }
NodePtr neg(NodePtr a) { return make_unary(NodeKind::Neg, std::move(a)); }

bool depends(const ExprNode& n, const std::string& var) {
    if (n.kind == NodeKind::Variable) return n.name == var;
    for (const auto& a : n.args)
        if (depends(*a, var)) return true;
    return false;
}

NodePtr diff(const NodePtr& n, const std::string& var) {
    if (!depends(*n, var)) return num(0.0);
    switch (n->kind) {
        case NodeKind::Number: return num(0.0);
        case NodeKind::Variable: return num(n->name == var ? 1.0 : 0.0);
        case NodeKind::Neg: return neg(diff(n->args[0], var));
        case NodeKind::Add: return add(diff(n->args[0], var), diff(n->args[1], var));
        case NodeKind::Sub: return sub(diff(n->args[0], var), diff(n->args[1], var));
        case NodeKind::Mul: {
            const NodePtr& u = n->args[0];
            const NodePtr& v = n->args[1];
            return add(mul(diff(u, var), v), mul(u, diff(v, var)));
        }
        case NodeKind::Div: {
            const NodePtr& u = n->args[0];
            const NodePtr& v = n->args[1];
            return divide(sub(mul(diff(u, var), v), mul(u, diff(v, var))), power(v, num(2.0)));
        }
        case NodeKind::Pow: {
            const NodePtr& u = n->args[0];
            const NodePtr& v = n->args[1];
            if (!depends(*v, var)) {
                // d/dx u^c = c u^(c-1) u'
                return mul(mul(v, power(u, sub(v, num(1.0)))), diff(u, var));
            }
            if (!depends(*u, var)) {
                // d/dx c^v = c^v ln(c) v'
                return mul(mul(n, call1("ln", u)), diff(v, var));
            }
            // general: u^v (v' ln u + v u'/u)
            return mul(n, add(mul(diff(v, var), call1("ln", u)), divide(mul(v, diff(u, var)), u)));
        }
        case NodeKind::Mod:
            throw ExpressionError("cannot differentiate 'mod'", 0);
        case NodeKind::Call: {
            const std::string name = n->name;
            const NodePtr& u = n->args[0];
            NodePtr du = diff(u, var);
            NodePtr outer;
            if (name == "sin") outer = call1("cos", u);
            else if (name == "cos") outer = neg(call1("sin", u));
            else if (name == "tan") outer = power(call1("sec", u), num(2.0));
            else if (name == "sec") outer = mul(call1("sec", u), call1("tan", u));
            else if (name == "csc") outer = neg(mul(call1("csc", u), call1("cot", u)));
            else if (name == "cot") outer = neg(power(call1("csc", u), num(2.0)));
            else if (name == "asin") outer = divide(num(1.0), call1("sqrt", sub(num(1.0), power(u, num(2.0)))));
            else if (name == "acos") outer = neg(divide(num(1.0), call1("sqrt", sub(num(1.0), power(u, num(2.0))))));
            else if (name == "atan") outer = divide(num(1.0), add(num(1.0), power(u, num(2.0))));
            else if (name == "acot") outer = neg(divide(num(1.0), add(num(1.0), power(u, num(2.0)))));
            else if (name == "asec") outer = divide(num(1.0), mul(call1("abs", u), call1("sqrt", sub(power(u, num(2.0)), num(1.0)))));
            else if (name == "acsc") outer = neg(divide(num(1.0), mul(call1("abs", u), call1("sqrt", sub(power(u, num(2.0)), num(1.0))))));
            else if (name == "sinh") outer = call1("cosh", u);
            else if (name == "cosh") outer = call1("sinh", u);
            else if (name == "tanh") outer = power(call1("sech", u), num(2.0));
            else if (name == "sech") outer = neg(mul(call1("sech", u), call1("tanh", u)));
            else if (name == "csch") outer = neg(mul(call1("csch", u), call1("coth", u)));
            else if (name == "coth") outer = neg(power(call1("csch", u), num(2.0)));
            else if (name == "asinh") outer = divide(num(1.0), call1("sqrt", add(power(u, num(2.0)), num(1.0))));
            else if (name == "acosh") outer = divide(num(1.0), call1("sqrt", sub(power(u, num(2.0)), num(1.0))));
            else if (name == "atanh" || name == "acoth") outer = divide(num(1.0), sub(num(1.0), power(u, num(2.0))));
            else if (name == "asech") outer = neg(divide(num(1.0), mul(u, call1("sqrt", sub(num(1.0), power(u, num(2.0)))))));
            else if (name == "acsch") outer = neg(divide(num(1.0), mul(call1("abs", u), call1("sqrt", add(num(1.0), power(u, num(2.0)))))));
            else if (name == "exp") outer = n;
            else if (name == "exp2") outer = mul(n, num(constants::ln2));
            else if (name == "expm1") outer = call1("exp", u);
            else if (name == "ln" || name == "log") outer = divide(num(1.0), u);
            else if (name == "log2") outer = divide(num(1.0), mul(u, num(constants::ln2)));
            else if (name == "log10") outer = divide(num(1.0), mul(u, num(constants::ln10)));
            else if (name == "log1p") outer = divide(num(1.0), add(num(1.0), u));
            else if (name == "sqrt") outer = divide(num(1.0), mul(num(2.0), n));
            else if (name == "cbrt") outer = divide(num(1.0), mul(num(3.0), power(n, num(2.0))));
            else if (name == "sq") outer = mul(num(2.0), u);
            else if (name == "abs") outer = call1("sign", u);
            else if (name == "gamma") outer = mul(n, call1("digamma", u));
            else if (name == "lgamma") outer = call1("digamma", u);
            else if (name == "erf") outer = mul(num(2.0 / sqrt(constants::pi)), call1("exp", neg(power(u, num(2.0)))));
            else if (name == "erfc") outer = neg(mul(num(2.0 / sqrt(constants::pi)), call1("exp", neg(power(u, num(2.0))))));
            else if (name == "normcdf") outer = call1("normpdf", u);
            else if (name == "normpdf") outer = neg(mul(u, n));
            else if (name == "deg") outer = num(constants::deg_per_rad);
            else if (name == "rad") outer = num(constants::rad_per_deg);
            else if (name == "floor" || name == "ceil" || name == "round" || name == "trunc" || name == "sign")
                outer = num(0.0);
            else if (name == "pow") {
                NodePtr p = power(n->args[0], n->args[1]);
                return diff(p, var);
            } else if (name == "hypot") {
                const NodePtr& a = n->args[0];
                const NodePtr& b = n->args[1];
                return divide(add(mul(a, diff(a, var)), mul(b, diff(b, var))), n);
            } else if (name == "atan2") {
                const NodePtr& y = n->args[0];
                const NodePtr& x = n->args[1];
                return divide(sub(mul(x, diff(y, var)), mul(y, diff(x, var))),
                              add(power(x, num(2.0)), power(y, num(2.0))));
            } else if (name == "logb") {
                // log_b(x) = ln x / ln b
                NodePtr q = divide(call1("ln", n->args[1]), call1("ln", n->args[0]));
                return diff(q, var);
            } else if (name == "root") {
                NodePtr p = power(n->args[0], divide(num(1.0), n->args[1]));
                return diff(p, var);
            } else if (name == "min" || name == "max" || name == "mod" || name == "fact" || name == "ncr" ||
                       name == "npr" || name == "gcd" || name == "lcm" || name == "isprime" || name == "fib" ||
                       name == "beta" || name == "digamma") {
                throw ExpressionError("cannot differentiate '" + name + "'", 0);
            } else {
                throw ExpressionError("no derivative rule for '" + name + "'", 0);
            }
            return mul(outer, du);
        }
    }
    return num(0.0);
}

NodePtr substitute_node(const NodePtr& n, const std::string& var, const NodePtr& with) {
    if (n->kind == NodeKind::Variable) return n->name == var ? with : n;
    if (n->args.empty()) return n;
    auto copy = std::make_shared<ExprNode>(n->kind);
    copy->value = n->value;
    copy->name = n->name;
    copy->fn = n->fn;
    for (const auto& a : n->args) copy->args.push_back(substitute_node(a, var, with));
    return copy;
}

void collect_variables(const ExprNode& n, std::set<std::string>& out) {
    if (n.kind == NodeKind::Variable) out.insert(n.name);
    for (const auto& a : n.args) collect_variables(*a, out);
}

std::size_t count_nodes(const ExprNode& n) {
    std::size_t c = 1;
    for (const auto& a : n.args) c += count_nodes(*a);
    return c;
}

// Compiled program op codes
enum CompOp { OP_PUSH = 0, OP_LOAD, OP_NEG, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_POW, OP_MOD, OP_CALL1, OP_CALL2 };

} // namespace

// ---------------------------------------------------------------------------
// Expression public API
// ---------------------------------------------------------------------------
Expression::Expression() : root_(make_number(0.0)) {}
Expression::Expression(std::shared_ptr<const ExprNode> root, std::string source)
    : root_(std::move(root)), source_(std::move(source)) {}

Expression Expression::parse(const std::string& text) { return parse(text, Limits()); }
Expression Expression::parse(const std::string& text, const Limits& limits) {
    if (text.size() > limits.max_length) throw ExpressionError("expression too long", 0);
    bool blank = true;
    for (char c : text)
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') { blank = false; break; }
    if (blank) throw ExpressionError("empty expression", 0);
    Parser p(text, limits);
    return Expression(p.parse(), text);
}
Expression Expression::constant(double v) { return Expression(make_number(v)); }
Expression Expression::variable(const std::string& name) { return Expression(make_variable(name)); }

bool Expression::is_constant() const { return root_->kind == NodeKind::Number; }
double Expression::constant_value() const { return root_->kind == NodeKind::Number ? root_->value : nan(); }

std::string Expression::to_string() const {
    std::string out;
    print_node(*root_, out);
    return out;
}

std::vector<std::string> Expression::variables() const {
    std::set<std::string> s;
    collect_variables(*root_, s);
    return std::vector<std::string>(s.begin(), s.end());
}
bool Expression::depends_on(const std::string& var) const { return depends(*root_, var); }
std::size_t Expression::node_count() const { return count_nodes(*root_); }

double Expression::evaluate(const Variables& vars) const { return eval_node(*root_, vars); }
double Expression::evaluate() const { return eval_node(*root_, Variables()); }
double Expression::evaluate(double x) const {
    Variables v;
    v["x"] = x;
    return eval_node(*root_, v);
}
double Expression::evaluate(double x, double y) const {
    Variables v;
    v["x"] = x;
    v["y"] = y;
    return eval_node(*root_, v);
}

Expression Expression::derivative(const std::string& var) const {
    return Expression(simplify(diff(root_, var)));
}
Expression Expression::simplified() const { return Expression(simplify(root_), source_); }
Expression Expression::substitute(const std::string& var, double value) const {
    return Expression(simplify(substitute_node(root_, var, make_number(value))));
}
Expression Expression::substitute(const std::string& var, const Expression& with) const {
    return Expression(substitute_node(root_, var, with.root_));
}

Expression::Compiled Expression::compile(const std::vector<std::string>& names) const {
    std::vector<int> ops, fns, slots;
    std::vector<double> values;
    struct Walker {
        static void walk(const ExprNode& n, const std::vector<std::string>& names, std::vector<int>& ops,
                         std::vector<int>& fns, std::vector<int>& slots, std::vector<double>& values) {
            auto emit = [&](int op, int fn, int slot, double v) {
                ops.push_back(op); fns.push_back(fn); slots.push_back(slot); values.push_back(v);
            };
            switch (n.kind) {
                case NodeKind::Number: emit(OP_PUSH, -1, -1, n.value); break;
                case NodeKind::Variable: {
                    int slot = -1;
                    for (std::size_t i = 0; i < names.size(); ++i)
                        if (names[i] == n.name) { slot = static_cast<int>(i); break; }
                    if (slot < 0) throw ExpressionError("unbound variable '" + n.name + "'", 0);
                    emit(OP_LOAD, -1, slot, 0.0);
                    break;
                }
                default: {
                    for (const auto& a : n.args) walk(*a, names, ops, fns, slots, values);
                    switch (n.kind) {
                        case NodeKind::Neg: emit(OP_NEG, -1, -1, 0.0); break;
                        case NodeKind::Add: emit(OP_ADD, -1, -1, 0.0); break;
                        case NodeKind::Sub: emit(OP_SUB, -1, -1, 0.0); break;
                        case NodeKind::Mul: emit(OP_MUL, -1, -1, 0.0); break;
                        case NodeKind::Div: emit(OP_DIV, -1, -1, 0.0); break;
                        case NodeKind::Pow: emit(OP_POW, -1, -1, 0.0); break;
                        case NodeKind::Mod: emit(OP_MOD, -1, -1, 0.0); break;
                        case NodeKind::Call:
                            emit(FUNCTIONS[n.fn].arity == 1 ? OP_CALL1 : OP_CALL2, n.fn, -1, 0.0);
                            break;
                        default: break;
                    }
                }
            }
        }
    };
    Walker::walk(*root_, names, ops, fns, slots, values);
    Compiled c;
    c.names_ = names;
    c.code_.reserve(ops.size());
    for (std::size_t i = 0; i < ops.size(); ++i) c.code_.push_back({ops[i], fns[i], slots[i], values[i]});
    return c;
}

double Expression::Compiled::operator()(const double* values, std::size_t count) const {
    double stack[256];
    std::size_t sp = 0;
    for (const Instr& in : code_) {
        switch (in.op) {
            case OP_PUSH: if (sp >= 256) throw ExpressionError("expression too deep to evaluate", 0); stack[sp++] = in.value; break;
            case OP_LOAD:
                if (static_cast<std::size_t>(in.slot) >= count) throw ExpressionError("missing variable value", 0);
                if (sp >= 256) throw ExpressionError("expression too deep to evaluate", 0);
                stack[sp++] = values[in.slot];
                break;
            case OP_NEG: stack[sp - 1] = -stack[sp - 1]; break;
            case OP_ADD: --sp; stack[sp - 1] = stack[sp - 1] + stack[sp]; break;
            case OP_SUB: --sp; stack[sp - 1] = stack[sp - 1] - stack[sp]; break;
            case OP_MUL: --sp; stack[sp - 1] = stack[sp - 1] * stack[sp]; break;
            case OP_DIV: --sp; stack[sp - 1] = stack[sp - 1] / stack[sp]; break;
            case OP_POW: --sp; stack[sp - 1] = pow(stack[sp - 1], stack[sp]); break;
            case OP_MOD: --sp; stack[sp - 1] = fmod(stack[sp - 1], stack[sp]); break;
            case OP_CALL1: stack[sp - 1] = FUNCTIONS[in.fn].f1(stack[sp - 1]); break;
            case OP_CALL2: --sp; stack[sp - 1] = FUNCTIONS[in.fn].f2(stack[sp - 1], stack[sp]); break;
            default: break;
        }
    }
    return sp ? stack[sp - 1] : nan();
}

std::function<double(double)> Expression::as_function(const std::string& var) const {
    Compiled c = compile({var});
    return [c](double x) { return c(x); };
}
std::function<double(double, double)> Expression::as_function2(const std::string& v1, const std::string& v2) const {
    Compiled c = compile({v1, v2});
    return [c](double x, double y) { return c(x, y); };
}
std::function<double(const std::vector<double>&)> Expression::as_function_n(const std::vector<std::string>& names) const {
    Compiled c = compile(names);
    return [c](const std::vector<double>& v) { return c(v.data(), v.size()); };
}

const std::vector<std::string>& Expression::function_names() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> v;
        for (int i = 0; i < FUNCTION_COUNT; ++i) v.push_back(FUNCTIONS[i].name);
        return v;
    }();
    return names;
}
const std::vector<std::string>& Expression::constant_names() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> v;
        for (int i = 0; i < CONSTANT_COUNT; ++i) v.push_back(CONSTANTS[i].name);
        return v;
    }();
    return names;
}
bool Expression::is_function(const std::string& name) { return find_function(name) >= 0; }
bool Expression::is_constant_name(const std::string& name) {
    double v;
    return lookup_constant(name, &v);
}

} // namespace cml
