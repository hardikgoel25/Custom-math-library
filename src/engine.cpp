// cml/engine.cpp — JSON compute engine.
#include "cml/engine.hpp"

#include "cml/core.hpp"
#include "cml/expression.hpp"
#include "cml/integrate.hpp"
#include "cml/interpolate.hpp"
#include "cml/matrix.hpp"
#include "cml/ode.hpp"
#include "cml/optimize.hpp"
#include "cml/polynomial.hpp"
#include "cml/roots.hpp"
#include "cml/special.hpp"
#include "cml/stats.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>   // only for the "compare against the platform libm" feature
#include <functional>
#include <map>
#include <stdexcept>

namespace cml {
namespace engine {

namespace {

const Limits LIMITS;

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------
struct EngineError : std::runtime_error {
    std::string code;
    std::string param;
    long position = -1;
    EngineError(std::string c, const std::string& msg, std::string p = "", long pos = -1)
        : std::runtime_error(msg), code(std::move(c)), param(std::move(p)), position(pos) {}
};

[[noreturn]] void invalid(const std::string& param, const std::string& msg) {
    throw EngineError("INVALID_PARAM", msg, param);
}

// ---------------------------------------------------------------------------
// Parameter access with validation
// ---------------------------------------------------------------------------
class Params {
public:
    explicit Params(const Json& j) : j_(j) {}

    bool has(const std::string& name) const {
        const Json* v = j_.find(name);
        return v && !v->is_null();
    }
    const Json& raw(const std::string& name) const { return j_.at(name); }

    double number(const std::string& name) const {
        const Json* v = j_.find(name);
        if (!v || v->is_null()) invalid(name, "missing required number '" + name + "'");
        return to_number(name, *v);
    }
    double number(const std::string& name, double def) const {
        const Json* v = j_.find(name);
        if (!v || v->is_null()) return def;
        return to_number(name, *v);
    }
    double finite(const std::string& name) const {
        const double v = number(name);
        if (!is_finite(v)) invalid(name, "'" + name + "' must be a finite number");
        return v;
    }
    double finite(const std::string& name, double def) const {
        const double v = number(name, def);
        if (!is_finite(v)) invalid(name, "'" + name + "' must be a finite number");
        return v;
    }
    double positive(const std::string& name, double def) const {
        const double v = finite(name, def);
        if (!(v > 0.0)) invalid(name, "'" + name + "' must be positive");
        return v;
    }
    int integer(const std::string& name, int def, int lo, int hi) const {
        const double v = number(name, static_cast<double>(def));
        if (!is_integer(v)) invalid(name, "'" + name + "' must be an integer");
        if (v < lo || v > hi) invalid(name, "'" + name + "' must be between " + std::to_string(lo) + " and " + std::to_string(hi));
        return static_cast<int>(v);
    }
    bool boolean(const std::string& name, bool def) const {
        const Json* v = j_.find(name);
        if (!v || v->is_null()) return def;
        if (!v->is_bool()) invalid(name, "'" + name + "' must be a boolean");
        return v->as_bool();
    }
    std::string string(const std::string& name) const {
        const Json* v = j_.find(name);
        if (!v || v->is_null()) invalid(name, "missing required string '" + name + "'");
        if (!v->is_string()) invalid(name, "'" + name + "' must be a string");
        return v->as_string();
    }
    std::string string(const std::string& name, const std::string& def) const {
        const Json* v = j_.find(name);
        if (!v || v->is_null()) return def;
        if (!v->is_string()) invalid(name, "'" + name + "' must be a string");
        return v->as_string();
    }
    std::string choice(const std::string& name, const std::string& def, const std::vector<std::string>& allowed) const {
        const std::string v = string(name, def);
        for (const auto& a : allowed)
            if (a == v) return v;
        std::string msg = "'" + name + "' must be one of: ";
        for (std::size_t i = 0; i < allowed.size(); ++i) msg += (i ? ", " : "") + allowed[i];
        invalid(name, msg);
    }
    std::vector<double> numbers(const std::string& name, std::size_t max_len, std::size_t min_len = 0) const {
        const Json* v = j_.find(name);
        if (!v || v->is_null()) {
            if (min_len == 0) return {};
            invalid(name, "missing required array '" + name + "'");
        }
        std::vector<double> out;
        if (v->is_string()) {
            out = parse_number_list(name, v->as_string());
        } else if (v->is_array()) {
            for (const Json& e : v->as_array()) out.push_back(to_number(name, e));
        } else {
            invalid(name, "'" + name + "' must be an array of numbers");
        }
        if (out.size() < min_len) invalid(name, "'" + name + "' needs at least " + std::to_string(min_len) + " values");
        if (out.size() > max_len) invalid(name, "'" + name + "' has too many values (limit " + std::to_string(max_len) + ")");
        for (double d : out)
            if (!is_finite(d)) invalid(name, "'" + name + "' must contain finite numbers");
        return out;
    }
    std::vector<std::string> strings(const std::string& name, std::size_t max_len) const {
        const Json* v = j_.find(name);
        if (!v || v->is_null()) return {};
        if (!v->is_array()) invalid(name, "'" + name + "' must be an array of strings");
        std::vector<std::string> out;
        for (const Json& e : v->as_array()) {
            if (!e.is_string()) invalid(name, "'" + name + "' must contain strings");
            out.push_back(e.as_string());
        }
        if (out.size() > max_len) invalid(name, "'" + name + "' has too many entries");
        return out;
    }
    Expression expression(const std::string& name) const {
        const std::string text = string(name);
        try {
            return Expression::parse(text);
        } catch (const ExpressionError& e) {
            throw EngineError("EXPRESSION_ERROR", std::string("in '") + name + "': " + e.what(), name, static_cast<long>(e.position()));
        }
    }
    Expression expression(const std::string& name, const std::string& def) const {
        if (!has(name)) return Expression::parse(def);
        return expression(name);
    }
    Polynomial polynomial(const std::string& name) const {
        const Json* v = j_.find(name);
        if (!v || v->is_null()) invalid(name, "missing required polynomial '" + name + "'");
        try {
            if (v->is_string()) {
                Polynomial p = Polynomial::parse(v->as_string());
                if (p.degree() > LIMITS.max_polynomial_degree) invalid(name, "polynomial degree limit exceeded");
                return p;
            }
            if (v->is_array()) {
                std::vector<double> c;
                for (const Json& e : v->as_array()) c.push_back(to_number(name, e));
                if (c.size() > static_cast<std::size_t>(LIMITS.max_polynomial_degree) + 1) invalid(name, "polynomial degree limit exceeded");
                for (double d : c)
                    if (!is_finite(d)) invalid(name, "coefficients must be finite");
                return Polynomial(c);
            }
            if (v->is_object()) {
                const Json* coeffs = v->find("coefficients");
                if (coeffs && coeffs->is_array()) {
                    std::vector<double> c;
                    for (const Json& e : coeffs->as_array()) c.push_back(to_number(name, e));
                    if (c.size() > static_cast<std::size_t>(LIMITS.max_polynomial_degree) + 1) invalid(name, "polynomial degree limit exceeded");
                    return Polynomial(c);
                }
            }
        } catch (const std::invalid_argument& e) {
            invalid(name, std::string("invalid polynomial: ") + e.what());
        } catch (const std::length_error& e) {
            invalid(name, std::string("invalid polynomial: ") + e.what());
        }
        invalid(name, "'" + name + "' must be a polynomial string like \"3x^2 - 2x + 1\" or a coefficient array [c0, c1, ...]");
    }
    Matrix matrix(const std::string& name) const {
        const Json* v = j_.find(name);
        if (!v || v->is_null()) invalid(name, "missing required matrix '" + name + "'");
        try {
            if (v->is_string()) {
                Matrix m = Matrix::parse(v->as_string());
                check_matrix(name, m);
                return m;
            }
            if (v->is_array()) {
                std::vector<std::vector<double>> rows;
                for (const Json& r : v->as_array()) {
                    if (!r.is_array()) invalid(name, "'" + name + "' must be an array of rows");
                    std::vector<double> row;
                    for (const Json& e : r.as_array()) row.push_back(to_number(name, e));
                    rows.push_back(row);
                    if (rows.size() > LIMITS.max_matrix_dim || row.size() > LIMITS.max_matrix_dim)
                        invalid(name, "matrix dimension limit (" + std::to_string(LIMITS.max_matrix_dim) + ") exceeded");
                }
                if (rows.empty()) invalid(name, "'" + name + "' must not be empty");
                Matrix m(rows);
                check_matrix(name, m);
                return m;
            }
        } catch (const std::invalid_argument& e) {
            invalid(name, std::string("invalid matrix: ") + e.what());
        } catch (const std::length_error& e) {
            invalid(name, std::string("invalid matrix: ") + e.what());
        }
        invalid(name, "'" + name + "' must be a matrix (array of rows or text like \"1 2; 3 4\")");
    }

private:
    const Json& j_;

    static double to_number(const std::string& name, const Json& v) {
        if (v.is_number()) return v.as_number();
        if (v.is_string()) {
            // Accept numeric strings and simple constant expressions such as "pi/2".
            try {
                Expression e = Expression::parse(v.as_string());
                if (!e.variables().empty()) invalid(name, "'" + name + "' must be a number (got an expression with variables)");
                return e.evaluate();
            } catch (const ExpressionError&) {
                invalid(name, "'" + name + "' must be a number");
            }
        }
        invalid(name, "'" + name + "' must be a number");
    }
    static std::vector<double> parse_number_list(const std::string& name, const std::string& text) {
        std::vector<double> out;
        std::string token;
        auto flush = [&]() {
            if (token.empty()) return;
            out.push_back(to_number(name, Json(token)));
            token.clear();
        };
        for (char c : text) {
            if (c == ',' || c == ' ' || c == '\n' || c == '\t' || c == ';' || c == '\r') flush();
            else token.push_back(c);
            if (out.size() > LIMITS.max_data_points) invalid(name, "too many values");
        }
        flush();
        return out;
    }
    static void check_matrix(const std::string& name, const Matrix& m) {
        if (m.rows() > LIMITS.max_matrix_dim || m.cols() > LIMITS.max_matrix_dim)
            invalid(name, "matrix dimension limit (" + std::to_string(LIMITS.max_matrix_dim) + ") exceeded");
        for (double d : m.data())
            if (!is_finite(d)) invalid(name, "matrix entries must be finite");
    }
};

// ---------------------------------------------------------------------------
// JSON conversion helpers
// ---------------------------------------------------------------------------
Json num(double v) { return is_finite(v) ? Json(v) : Json(); }

Json matrix_json(const Matrix& m) { return Json(m.to_rows()); }

Json root_json(const RootResult& r) {
    Json j = Json::object();
    j["method"] = r.method;
    j["root"] = num(r.root);
    j["fRoot"] = num(r.f_root);
    j["iterations"] = r.iterations;
    j["evaluations"] = r.evaluations;
    j["converged"] = r.converged;
    j["message"] = r.message;
    Json hist = Json::array();
    for (const RootStep& s : r.history) {
        Json step = Json::object();
        step["iteration"] = s.iteration;
        step["a"] = num(s.a);
        step["b"] = num(s.b);
        step["x"] = num(s.x);
        step["fx"] = num(s.fx);
        step["error"] = num(s.error);
        hist.push_back(step);
    }
    j["history"] = std::move(hist);
    return j;
}

Json min_json(const MinResult& r) {
    Json j = Json::object();
    j["method"] = r.method;
    j["x"] = num(r.x);
    j["fx"] = num(r.fx);
    j["iterations"] = r.iterations;
    j["evaluations"] = r.evaluations;
    j["converged"] = r.converged;
    j["message"] = r.message;
    Json hist = Json::array();
    for (const MinStep& s : r.history) {
        Json step = Json::object();
        step["iteration"] = s.iteration;
        step["a"] = num(s.a);
        step["b"] = num(s.b);
        step["x"] = num(s.x);
        step["fx"] = num(s.fx);
        hist.push_back(step);
    }
    j["history"] = std::move(hist);
    return j;
}

Json minn_json(const MinResultN& r) {
    Json j = Json::object();
    j["method"] = r.method;
    j["x"] = Json(r.x);
    j["fx"] = num(r.fx);
    j["iterations"] = r.iterations;
    j["evaluations"] = r.evaluations;
    j["converged"] = r.converged;
    j["message"] = r.message;
    j["path"] = Json(r.path);
    j["pathFx"] = Json(r.path_fx);
    return j;
}

Json integral_json(const IntegralResult& r) {
    Json j = Json::object();
    j["method"] = r.method;
    j["value"] = num(r.value);
    j["errorEstimate"] = num(r.error_estimate);
    j["evaluations"] = r.evaluations;
    j["subdivisions"] = r.subdivisions;
    j["converged"] = r.converged;
    j["message"] = r.message;
    if (!r.table.empty()) j["table"] = Json(r.table);
    return j;
}

Json poly_json(const Polynomial& p) {
    Json j = Json::object();
    j["coefficients"] = Json(p.coefficients());
    j["degree"] = p.degree();
    j["text"] = p.to_string();
    j["unicode"] = p.to_unicode();
    return j;
}

Json complex_json(const std::complex<double>& z) {
    Json j = Json::object();
    j["re"] = num(z.real());
    j["im"] = num(z.imag());
    return j;
}

Json table_json(const std::function<double(double)>& f, double from, double to, int samples) {
    Json xs = Json::array(), ys = Json::array();
    for (int i = 0; i < samples; ++i) {
        const double x = (samples == 1) ? from : from + (to - from) * i / (samples - 1);
        double y;
        try { y = f(x); } catch (const std::exception&) { y = nan(); }
        xs.push_back(num(x));
        ys.push_back(num(y)); // non-finite -> null
    }
    Json j = Json::object();
    j["x"] = std::move(xs);
    j["y"] = std::move(ys);
    return j;
}

// Build a single-variable function from an expression param, picking the
// variable automatically when the expression has exactly one.
struct ScalarFn {
    Expression expr;
    std::string var;
    std::function<double(double)> fn;
};
ScalarFn scalar_function(const Params& p, const std::string& name, const std::string& var_param = "var") {
    ScalarFn s;
    s.expr = p.expression(name);
    const std::vector<std::string> vars = s.expr.variables();
    s.var = p.string(var_param, "");
    if (s.var.empty()) {
        if (vars.empty()) s.var = "x";
        else if (vars.size() == 1) s.var = vars[0];
        else {
            bool has_x = false;
            for (const auto& v : vars) if (v == "x") has_x = true;
            if (!has_x) {
                std::string list;
                for (std::size_t i = 0; i < vars.size(); ++i) list += (i ? ", " : "") + vars[i];
                invalid(name, "expression has several variables (" + list + "); specify which one with '" + var_param + "'");
            }
            s.var = "x";
        }
    }
    for (const auto& v : vars)
        if (v != s.var) invalid(name, "expression uses variable '" + v + "' which is not bound (only '" + s.var + "' is)");
    try {
        s.fn = s.expr.as_function(s.var);
    } catch (const ExpressionError& e) {
        throw EngineError("EXPRESSION_ERROR", e.what(), name);
    }
    return s;
}

RootOptions root_options(const Params& p) {
    RootOptions o;
    o.tol = p.positive("tol", 1e-12);
    o.max_iter = p.integer("maxIter", 200, 1, LIMITS.max_iterations);
    o.record_history = p.boolean("history", true);
    o.max_history = p.integer("maxHistory", 2000, 0, 100000);
    return o;
}
MinOptions min_options(const Params& p) {
    MinOptions o;
    o.tol = p.positive("tol", 1e-8);
    o.max_iter = p.integer("maxIter", 500, 1, LIMITS.max_iterations);
    o.record_history = p.boolean("history", true);
    o.max_history = p.integer("maxHistory", 2000, 0, 100000);
    return o;
}

// ---------------------------------------------------------------------------
// Op registry
// ---------------------------------------------------------------------------
using Handler = std::function<Json(const Params&)>;

struct Registry {
    std::vector<OpInfo> infos;
    std::map<std::string, Handler> handlers;
    void add(const std::string& name, const std::string& description, Json params, Handler h) {
        OpInfo info;
        info.name = name;
        info.category = name.substr(0, name.find('.'));
        info.description = description;
        info.params = std::move(params);
        infos.push_back(info);
        handlers[name] = std::move(h);
    }
};

Json P(std::initializer_list<std::pair<const char*, const char*>> items) {
    // Compact param schema: name -> "type: description". Required params are marked with '*'.
    Json j = Json::object();
    for (const auto& kv : items) j[kv.first] = kv.second;
    return j;
}

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------
Json op_meta_ops(const Params&);

Json op_expression_evaluate(const Params& p) {
    Expression e = p.expression("expr");
    Expression::Variables vars;
    if (p.has("vars")) {
        const Json& v = p.raw("vars");
        if (!v.is_object()) invalid("vars", "'vars' must be an object of name -> number");
        for (const auto& kv : v.as_object()) vars[kv.first] = Params(v).number(kv.first);
    }
    if (p.has("x")) vars["x"] = p.number("x");
    if (p.has("y")) vars["y"] = p.number("y");
    if (p.has("t")) vars["t"] = p.number("t");
    Json r = Json::object();
    try {
        const double value = e.evaluate(vars);
        r["value"] = num(value);
        r["text"] = to_string(value, 15);
        r["finite"] = is_finite(value);
    } catch (const ExpressionError& ex) {
        throw EngineError("EXPRESSION_ERROR", ex.what(), "expr");
    }
    r["expression"] = e.to_string();
    r["variables"] = Json(e.variables());
    return r;
}

Json op_expression_parse(const Params& p) {
    Expression e = p.expression("expr");
    Json r = Json::object();
    r["expression"] = e.to_string();
    r["simplified"] = e.simplified().to_string();
    r["variables"] = Json(e.variables());
    r["nodes"] = static_cast<double>(e.node_count());
    r["isConstant"] = e.is_constant();
    if (e.is_constant()) r["value"] = num(e.constant_value());
    return r;
}

Json op_expression_derivative(const Params& p) {
    Expression e = p.expression("expr");
    const std::string var = p.string("var", "x");
    const int order = p.integer("order", 1, 1, 6);
    Json r = Json::object();
    Json chain = Json::array();
    Expression d = e;
    try {
        for (int i = 0; i < order; ++i) {
            d = d.derivative(var);
            chain.push_back(d.to_string());
        }
    } catch (const ExpressionError& ex) {
        throw EngineError("EXPRESSION_ERROR", ex.what(), "expr");
    }
    r["expression"] = e.to_string();
    r["derivative"] = d.to_string();
    r["derivatives"] = chain;
    r["var"] = var;
    if (p.has("at")) {
        Expression::Variables vars;
        vars[var] = p.number("at");
        try { r["value"] = num(d.evaluate(vars)); }
        catch (const ExpressionError& ex) { throw EngineError("EXPRESSION_ERROR", ex.what(), "at"); }
    }
    return r;
}

Json op_expression_table(const Params& p) {
    ScalarFn f = scalar_function(p, "expr");
    const double from = p.finite("from", -10.0), to = p.finite("to", 10.0);
    const int samples = p.integer("samples", 500, 1, LIMITS.max_samples);
    Json r = table_json(f.fn, from, to, samples);
    r["expression"] = f.expr.to_string();
    r["var"] = f.var;
    if (p.boolean("derivative", false)) {
        try {
            Expression d = f.expr.derivative(f.var);
            r["derivative"] = d.to_string();
            r["dy"] = table_json(d.as_function(f.var), from, to, samples)["y"];
        } catch (const ExpressionError& ex) {
            r["derivativeError"] = ex.what();
        }
    }
    return r;
}

Json op_expression_functions(const Params&) {
    Json r = Json::object();
    r["functions"] = Json(Expression::function_names());
    r["constants"] = Json(Expression::constant_names());
    return r;
}

// core -------------------------------------------------------------------
struct CoreFn { const char* name; double (*f)(double); double (*ref)(double); };
double ref_cot(double x) { return 1.0 / std::tan(x); }
double ref_sec(double x) { return 1.0 / std::cos(x); }
double ref_csc(double x) { return 1.0 / std::sin(x); }
double ref_acot(double x) { return x == 0.0 ? 1.5707963267948966 : std::atan(1.0 / x); }
double ref_asec(double x) { return std::acos(1.0 / x); }
double ref_acsc(double x) { return std::asin(1.0 / x); }
double ref_coth(double x) { return 1.0 / std::tanh(x); }
double ref_sech(double x) { return 1.0 / std::cosh(x); }
double ref_csch(double x) { return 1.0 / std::sinh(x); }
double ref_acoth(double x) { return std::atanh(1.0 / x); }
double ref_asech(double x) { return std::acosh(1.0 / x); }
double ref_acsch(double x) { return std::asinh(1.0 / x); }
double ref_ln(double x) { return std::log(x); }
double ref_gamma(double x) { return std::tgamma(x); }
double ref_lgamma(double x) { return std::lgamma(x); }
double ref_none(double) { return std::nan(""); }
double ref_factorial(double x) { return std::tgamma(x + 1.0); }
double ref_normcdf(double x) { return 0.5 * std::erfc(-x / 1.4142135623730951); }
double cml_normcdf(double x) { return normal_cdf(x); }
double cml_normpdf(double x) { return normal_pdf(x); }
double ref_normpdf(double x) { return std::exp(-0.5 * x * x) / 2.5066282746310002; }
double cml_sign(double x) { return static_cast<double>(sign(x)); }

const CoreFn CORE_FNS[] = {
    {"sin", sin, std::sin},       {"cos", cos, std::cos},       {"tan", tan, std::tan},
    {"cot", cot, ref_cot},        {"sec", sec, ref_sec},        {"csc", csc, ref_csc},
    {"asin", asin, std::asin},    {"acos", acos, std::acos},    {"atan", atan, std::atan},
    {"acot", acot, ref_acot},     {"asec", asec, ref_asec},     {"acsc", acsc, ref_acsc},
    {"sinh", sinh, std::sinh},    {"cosh", cosh, std::cosh},    {"tanh", tanh, std::tanh},
    {"coth", coth, ref_coth},     {"sech", sech, ref_sech},     {"csch", csch, ref_csch},
    {"asinh", asinh, std::asinh}, {"acosh", acosh, std::acosh}, {"atanh", atanh, std::atanh},
    {"acoth", acoth, ref_acoth},  {"asech", asech, ref_asech},  {"acsch", acsch, ref_acsch},
    {"exp", exp, std::exp},       {"exp2", exp2, std::exp2},    {"expm1", expm1, std::expm1},
    {"ln", ln, ref_ln},           {"log2", log2, std::log2},    {"log10", log10, std::log10},
    {"log1p", log1p, std::log1p}, {"sqrt", sqrt, std::sqrt},    {"cbrt", cbrt, std::cbrt},
    {"abs", abs, std::fabs},      {"floor", floor, std::floor}, {"ceil", ceil, std::ceil},
    {"round", round, std::round}, {"trunc", trunc, std::trunc}, {"sign", cml_sign, ref_none},
    {"gamma", gamma, ref_gamma},  {"lgamma", lgamma, ref_lgamma}, {"digamma", digamma, ref_none},
    {"erf", erf, std::erf},       {"erfc", erfc, std::erfc},    {"factorial", factorial, ref_factorial},
    {"normcdf", cml_normcdf, ref_normcdf}, {"normpdf", cml_normpdf, ref_normpdf},
};
const int CORE_FN_COUNT = static_cast<int>(sizeof(CORE_FNS) / sizeof(CORE_FNS[0]));

const CoreFn& find_core_fn(const std::string& name) {
    for (int i = 0; i < CORE_FN_COUNT; ++i)
        if (name == CORE_FNS[i].name) return CORE_FNS[i];
    std::string list;
    for (int i = 0; i < CORE_FN_COUNT; ++i) list += std::string(i ? ", " : "") + CORE_FNS[i].name;
    invalid("fn", "unknown function '" + name + "'. Available: " + list);
}

double ulp_distance(double a, double b) {
    if (a == b) return 0.0;
    if (!std::isfinite(a) || !std::isfinite(b)) return std::isnan(a) && std::isnan(b) ? 0.0 : std::numeric_limits<double>::infinity();
    const double scale = std::fabs(std::nextafter(b, std::numeric_limits<double>::infinity()) - b);
    return std::fabs(a - b) / scale;
}

Json op_core_evaluate(const Params& p) {
    const CoreFn& f = find_core_fn(p.string("fn"));
    const double x = p.number("x");
    const double v = f.f(x);
    Json r = Json::object();
    r["fn"] = f.name;
    r["x"] = num(x);
    r["value"] = num(v);
    r["text"] = to_string(v, 17);
    return r;
}

Json op_core_compare(const Params& p) {
    const CoreFn& f = find_core_fn(p.string("fn"));
    const double from = p.finite("from", -5.0), to = p.finite("to", 5.0);
    const int samples = p.integer("samples", 200, 1, 20000);
    Json rows = Json::array();
    double worst = 0.0, worst_x = 0.0;
    double sum = 0.0;
    int counted = 0;
    for (int i = 0; i < samples; ++i) {
        const double x = (samples == 1) ? from : from + (to - from) * i / (samples - 1);
        const double a = f.f(x), b = f.ref(x);
        const double u = ulp_distance(a, b);
        Json row = Json::object();
        row["x"] = num(x);
        row["cml"] = num(a);
        row["libm"] = num(b);
        row["ulps"] = std::isfinite(u) ? Json(u) : Json();
        rows.push_back(row);
        if (std::isfinite(u)) {
            sum += u;
            ++counted;
            if (u > worst) { worst = u; worst_x = x; }
        }
    }
    Json r = Json::object();
    r["fn"] = f.name;
    r["rows"] = std::move(rows);
    r["worstUlps"] = num(worst);
    r["worstAt"] = num(worst_x);
    r["meanUlps"] = counted ? num(sum / counted) : Json();
    r["hasReference"] = f.ref != ref_none;
    return r;
}

Json op_core_constants(const Params&) {
    using namespace constants;
    Json r = Json::object();
    const std::pair<const char*, double> items[] = {
        {"pi", pi}, {"tau", tau}, {"e", e}, {"phi", phi}, {"sqrt2", sqrt2}, {"sqrt3", sqrt3}, {"sqrt5", sqrt5},
        {"ln2", ln2}, {"ln10", ln10}, {"log2e", log2e}, {"log10e", log10e}, {"eulerGamma", euler_gamma},
        {"epsilon", epsilon}, {"maxValue", max_value}, {"minNormal", min_normal},
    };
    for (const auto& kv : items) r[kv.first] = num(kv.second);
    return r;
}

Json op_core_functions(const Params&) {
    Json r = Json::array();
    for (int i = 0; i < CORE_FN_COUNT; ++i) r.push_back(CORE_FNS[i].name);
    Json out = Json::object();
    out["functions"] = r;
    return out;
}

// special ----------------------------------------------------------------
Json op_special_number_theory(const Params& p) {
    const std::string op = p.choice("fn", "", {"factorial", "binomial", "permutations", "gcd", "lcm", "isPrime", "primeFactors", "primes", "fibonacci", "beta"});
    Json r = Json::object();
    r["fn"] = op;
    auto u64 = [&](const char* name, double max) {
        const double v = p.number(name);
        if (!is_integer(v) || v < 0 || v > max) invalid(name, std::string("'") + name + "' must be a non-negative integer <= " + to_string(max, 17));
        return static_cast<std::uint64_t>(v);
    };
    try {
        if (op == "factorial") {
            const double n = p.number("n");
            r["value"] = num(factorial(n));
            std::uint64_t exact;
            if (is_integer(n) && n >= 0 && n <= 20 && factorial_exact(static_cast<unsigned>(n), &exact)) r["exact"] = std::to_string(exact);
        } else if (op == "binomial") {
            r["value"] = num(binomial(p.number("n"), p.number("k")));
        } else if (op == "permutations") {
            r["value"] = num(permutations(p.number("n"), p.number("k")));
        } else if (op == "gcd") {
            r["value"] = num(static_cast<double>(gcd(u64("a", 9e15), u64("b", 9e15))));
        } else if (op == "lcm") {
            r["value"] = num(static_cast<double>(lcm(u64("a", 9e15), u64("b", 9e15))));
        } else if (op == "isPrime") {
            r["value"] = is_prime(u64("n", 1.8e19));
        } else if (op == "primeFactors") {
            const auto f = prime_factors(u64("n", 9e15));
            Json arr = Json::array();
            for (auto v : f) arr.push_back(static_cast<double>(v));
            r["factors"] = arr;
        } else if (op == "primes") {
            const auto ps = primes_up_to(u64("n", 2000000));
            Json arr = Json::array();
            for (auto v : ps) arr.push_back(static_cast<double>(v));
            r["primes"] = arr;
            r["count"] = static_cast<double>(ps.size());
        } else if (op == "fibonacci") {
            const std::uint64_t n = u64("n", 93);
            const std::uint64_t v = fibonacci(static_cast<unsigned>(n));
            r["value"] = num(static_cast<double>(v));
            r["exact"] = std::to_string(v);
        } else if (op == "beta") {
            r["value"] = num(beta(p.number("a"), p.number("b")));
        }
    } catch (const std::overflow_error& e) {
        throw EngineError("NUMERIC_ERROR", e.what());
    } catch (const std::length_error& e) {
        throw EngineError("LIMIT_EXCEEDED", e.what());
    }
    return r;
}

// roots --------------------------------------------------------------------
const std::vector<std::string> ROOT_METHODS = {"bisection", "regula_falsi", "secant", "newton", "halley", "brent",
                                               "ridders", "steffensen", "fixed_point", "linear_search", "variable_step"};

RootResult run_root_method(const std::string& method, const ScalarFn& f, const Params& p, const RootOptions& opt, Json& extra) {
    const bool bracket = method == "bisection" || method == "regula_falsi" || method == "brent" || method == "ridders" ||
                         method == "linear_search" || method == "variable_step";
    if (bracket) {
        const double a = p.finite("a"), b = p.finite("b");
        if (method == "bisection") return bisection(f.fn, a, b, opt);
        if (method == "regula_falsi") return regula_falsi(f.fn, a, b, opt);
        if (method == "brent") return brent(f.fn, a, b, opt);
        if (method == "ridders") return ridders(f.fn, a, b, opt);
        if (method == "linear_search") return linear_search(f.fn, a, b, p.positive("step", abs(b - a) / 100.0), opt);
        return variable_step(f.fn, a, b, p.positive("step", abs(b - a) / 100.0), opt);
    }
    if (method == "secant") {
        const double x0 = p.finite("x0");
        const double x1 = p.has("x1") ? p.finite("x1") : (p.has("b") ? p.finite("b") : x0 + 0.1 * fmax(1.0, abs(x0)));
        return secant(f.fn, x0, x1, opt);
    }
    if (method == "newton" || method == "halley") {
        const double x0 = p.finite("x0");
        Expression d1, d2;
        bool symbolic = true;
        try {
            d1 = p.has("df") ? p.expression("df") : f.expr.derivative(f.var);
            if (method == "halley") d2 = p.has("d2f") ? p.expression("d2f") : d1.derivative(f.var);
        } catch (const ExpressionError& e) {
            symbolic = false;
            extra["derivativeNote"] = std::string("symbolic derivative unavailable (") + e.what() + "); using numerical derivative";
        }
        if (symbolic) {
            extra["derivative"] = d1.to_string();
            auto dfn = d1.as_function(f.var);
            if (method == "newton") return newton(f.fn, dfn, x0, opt);
            extra["secondDerivative"] = d2.to_string();
            auto d2fn = d2.as_function(f.var);
            return halley(f.fn, dfn, d2fn, x0, opt);
        }
        if (method == "newton") return newton(f.fn, x0, opt);
        auto fn = f.fn;
        RealFn dfn = [fn](double x) { return derivative(fn, x, 1e-5 * fmax(1.0, abs(x))); };
        RealFn d2fn = [fn](double x) { return second_derivative(fn, x, 1e-4 * fmax(1.0, abs(x))); };
        return halley(f.fn, dfn, d2fn, x0, opt);
    }
    if (method == "steffensen") return steffensen(f.fn, p.finite("x0"), opt);
    // fixed_point: solve x = g(x); if the user gave f(x) = 0 we use g(x) = x + f(x)? No: require g explicitly.
    return fixed_point(f.fn, p.finite("x0"), opt);
}

Json op_roots_find(const Params& p) {
    const std::string method = p.choice("method", "brent", ROOT_METHODS);
    ScalarFn f = scalar_function(p, "f");
    const RootOptions opt = root_options(p);
    Json extra = Json::object();
    RootResult r = run_root_method(method, f, p, opt, extra);
    Json j = root_json(r);
    j["expression"] = f.expr.to_string();
    j["var"] = f.var;
    for (const auto& kv : extra.as_object()) j[kv.first] = kv.second;
    return j;
}

Json op_roots_compare(const Params& p) {
    ScalarFn f = scalar_function(p, "f");
    RootOptions opt = root_options(p);
    opt.record_history = false;
    Json results = Json::array();
    for (const std::string& m : ROOT_METHODS) {
        if (m == "fixed_point") continue;
        const bool bracket = m == "bisection" || m == "regula_falsi" || m == "brent" || m == "ridders" ||
                             m == "linear_search" || m == "variable_step";
        if (bracket && !(p.has("a") && p.has("b"))) continue;
        if (!bracket && !p.has("x0")) continue;
        Json extra = Json::object();
        try {
            RootResult r = run_root_method(m, f, p, opt, extra);
            Json j = root_json(r);
            j.as_object().erase(std::remove_if(j.as_object().begin(), j.as_object().end(),
                                               [](const std::pair<std::string, Json>& kv) { return kv.first == "history"; }),
                                j.as_object().end());
            results.push_back(j);
        } catch (const std::exception& e) {
            Json j = Json::object();
            j["method"] = m;
            j["converged"] = false;
            j["message"] = e.what();
            results.push_back(j);
        }
    }
    Json out = Json::object();
    out["expression"] = f.expr.to_string();
    out["results"] = results;
    return out;
}

Json op_roots_all(const Params& p) {
    ScalarFn f = scalar_function(p, "f");
    const double a = p.finite("a"), b = p.finite("b");
    const int samples = p.integer("samples", 1000, 2, LIMITS.max_samples);
    RootOptions opt = root_options(p);
    opt.record_history = false;
    const std::vector<RootResult> rs = find_all_roots(f.fn, a, b, samples, opt);
    Json arr = Json::array();
    for (const RootResult& r : rs) {
        Json j = Json::object();
        j["root"] = num(r.root);
        j["fRoot"] = num(r.f_root);
        j["iterations"] = r.iterations;
        j["converged"] = r.converged;
        arr.push_back(j);
    }
    Json out = Json::object();
    out["expression"] = f.expr.to_string();
    out["roots"] = arr;
    out["count"] = static_cast<double>(rs.size());
    return out;
}

// polynomial ---------------------------------------------------------------
Json op_polynomial_parse(const Params& p) { return poly_json(p.polynomial("p")); }

Json op_polynomial_evaluate(const Params& p) {
    const Polynomial poly = p.polynomial("p");
    Json r = poly_json(poly);
    if (p.has("x")) {
        const double x = p.finite("x");
        const auto pd = poly.evaluate_with_derivative(x);
        r["x"] = num(x);
        r["value"] = num(pd.first);
        r["derivativeValue"] = num(pd.second);
    }
    if (p.has("xs")) {
        const std::vector<double> xs = p.numbers("xs", LIMITS.max_samples);
        Json vals = Json::array();
        for (double x : xs) vals.push_back(num(poly.evaluate(x)));
        r["values"] = vals;
    }
    return r;
}

Json op_polynomial_roots(const Params& p) {
    const Polynomial poly = p.polynomial("p");
    const double tol = p.positive("tol", 1e-14);
    const int max_iter = p.integer("maxIter", 500, 1, 20000);
    const Polynomial::RootsResult rr = poly.roots(tol, max_iter);
    Json r = poly_json(poly);
    Json roots = Json::array();
    for (const auto& z : rr.roots) {
        Json c = complex_json(z);
        const auto pv = poly.evaluate(z);
        c["residual"] = num(hypot(pv.real(), pv.imag()));
        roots.push_back(c);
    }
    r["roots"] = roots;
    r["realRoots"] = Json(rr.real_roots);
    r["iterations"] = rr.iterations;
    r["converged"] = rr.converged;
    return r;
}

Json op_polynomial_calculus(const Params& p) {
    const Polynomial poly = p.polynomial("p");
    const std::string op = p.choice("fn", "derivative", {"derivative", "integral", "definite_integral"});
    Json r = Json::object();
    r["input"] = poly_json(poly);
    if (op == "derivative") {
        const int order = p.integer("order", 1, 1, 50);
        Polynomial d = poly;
        for (int i = 0; i < order; ++i) d = d.derivative();
        r["result"] = poly_json(d);
    } else if (op == "integral") {
        r["result"] = poly_json(poly.integral(p.finite("constant", 0.0)));
    } else {
        const double a = p.finite("a"), b = p.finite("b");
        r["result"] = poly_json(poly.integral());
        r["value"] = num(poly.definite_integral(a, b));
    }
    return r;
}

Json op_polynomial_arithmetic(const Params& p) {
    const Polynomial a = p.polynomial("a");
    const std::string op = p.choice("fn", "add", {"add", "subtract", "multiply", "divide", "modulo", "gcd", "compose", "power", "scale"});
    Json r = Json::object();
    r["a"] = poly_json(a);
    try {
        if (op == "power") {
            const int n = p.integer("n", 2, 0, 64);
            if (a.degree() * n > LIMITS.max_polynomial_degree) throw EngineError("LIMIT_EXCEEDED", "result degree exceeds the limit");
            r["result"] = poly_json(a.pow(static_cast<unsigned>(n)));
            return r;
        }
        if (op == "scale") {
            r["result"] = poly_json(a * p.finite("k", 1.0));
            return r;
        }
        const Polynomial b = p.polynomial("b");
        r["b"] = poly_json(b);
        if (op == "add") r["result"] = poly_json(a + b);
        else if (op == "subtract") r["result"] = poly_json(a - b);
        else if (op == "multiply") r["result"] = poly_json(a * b);
        else if (op == "divide") {
            const auto qr = Polynomial::divmod(a, b);
            r["quotient"] = poly_json(qr.first);
            r["remainder"] = poly_json(qr.second);
            r["result"] = poly_json(qr.first);
        } else if (op == "modulo") r["result"] = poly_json(a % b);
        else if (op == "gcd") r["result"] = poly_json(Polynomial::gcd(a, b));
        else if (op == "compose") {
            if (a.degree() * fmax(1, b.degree()) > LIMITS.max_polynomial_degree) throw EngineError("LIMIT_EXCEEDED", "result degree exceeds the limit");
            r["result"] = poly_json(a.compose(b));
        }
    } catch (const std::invalid_argument& e) {
        throw EngineError("NUMERIC_ERROR", e.what());
    } catch (const std::length_error& e) {
        throw EngineError("LIMIT_EXCEEDED", e.what());
    }
    return r;
}

Json op_polynomial_table(const Params& p) {
    const Polynomial poly = p.polynomial("p");
    const double from = p.finite("from", -10.0), to = p.finite("to", 10.0);
    const int samples = p.integer("samples", 500, 1, LIMITS.max_samples);
    Json r = table_json([&poly](double x) { return poly.evaluate(x); }, from, to, samples);
    r["polynomial"] = poly_json(poly);
    return r;
}

Json op_polynomial_from_roots(const Params& p) {
    const std::vector<double> roots = p.numbers("roots", static_cast<std::size_t>(LIMITS.max_polynomial_degree), 1);
    return poly_json(Polynomial::from_roots(roots, p.finite("leading", 1.0)));
}

// matrix -------------------------------------------------------------------
Json op_matrix_compute(const Params& p) {
    const std::string op = p.choice("fn", "determinant",
        {"determinant", "inverse", "transpose", "rank", "rref", "lu", "qr", "cholesky", "eigen", "solve", "multiply",
         "add", "subtract", "hadamard", "trace", "norms", "power", "cofactor", "adjugate", "condition", "scale", "info",
         "least_squares", "power_iteration"});
    const Matrix A = p.matrix("A");
    Json r = Json::object();
    r["fn"] = op;
    r["rows"] = static_cast<double>(A.rows());
    r["cols"] = static_cast<double>(A.cols());
    try {
        if (op == "determinant") r["value"] = num(A.determinant());
        else if (op == "inverse") r["matrix"] = matrix_json(A.inverse());
        else if (op == "transpose") r["matrix"] = matrix_json(A.transpose());
        else if (op == "rank") r["value"] = static_cast<double>(A.rank(p.positive("tol", 1e-12)));
        else if (op == "rref") {
            const Matrix::RREF f = A.rref(p.positive("tol", 1e-12));
            r["matrix"] = matrix_json(f.matrix);
            r["rank"] = static_cast<double>(f.rank);
            Json piv = Json::array();
            for (auto c : f.pivots) piv.push_back(static_cast<double>(c));
            r["pivots"] = piv;
        } else if (op == "lu") {
            const Matrix::LU f = A.lu();
            r["L"] = matrix_json(f.L);
            r["U"] = matrix_json(f.U);
            Json perm = Json::array();
            Matrix P(A.rows(), A.rows(), 0.0);
            for (std::size_t i = 0; i < f.perm.size(); ++i) { perm.push_back(static_cast<double>(f.perm[i])); P(i, f.perm[i]) = 1.0; }
            r["P"] = matrix_json(P);
            r["permutation"] = perm;
            r["sign"] = static_cast<double>(f.sign);
            r["singular"] = f.singular;
        } else if (op == "qr") {
            const Matrix::QR f = A.qr();
            r["Q"] = matrix_json(f.Q);
            r["R"] = matrix_json(f.R);
        } else if (op == "cholesky") {
            r["L"] = matrix_json(A.cholesky());
        } else if (op == "eigen") {
            if (A.is_symmetric(1e-12)) {
                const Matrix::Eigen e = A.eigen_symmetric();
                r["symmetric"] = true;
                r["values"] = Json(e.values);
                r["imag"] = Json(std::vector<double>(e.values.size(), 0.0));
                r["vectors"] = matrix_json(e.vectors);
                r["iterations"] = e.iterations;
                r["converged"] = e.converged;
            } else {
                const Matrix::Eigen e = A.eigenvalues();
                r["symmetric"] = false;
                r["values"] = Json(e.values);
                r["imag"] = Json(e.imag);
                r["iterations"] = e.iterations;
                r["converged"] = e.converged;
            }
        } else if (op == "solve") {
            if (p.has("B")) {
                const Matrix B = p.matrix("B");
                r["matrix"] = matrix_json(A.solve(B));
            } else {
                const std::vector<double> b = p.numbers("b", LIMITS.max_matrix_dim, 1);
                const std::vector<double> x = A.solve(b);
                r["x"] = Json(x);
                const std::vector<double> ax = A * x;
                double res = 0.0;
                for (std::size_t i = 0; i < ax.size(); ++i) res += (ax[i] - b[i]) * (ax[i] - b[i]);
                r["residual"] = num(sqrt(res));
            }
        } else if (op == "least_squares") {
            const std::vector<double> b = p.numbers("b", LIMITS.max_matrix_dim, 1);
            r["x"] = Json(A.solve_least_squares(b));
        } else if (op == "multiply" || op == "add" || op == "subtract" || op == "hadamard") {
            const Matrix B = p.matrix("B");
            if (op == "multiply") r["matrix"] = matrix_json(p.boolean("strassen", false) ? A.multiply_strassen(B) : A * B);
            else if (op == "add") r["matrix"] = matrix_json(A + B);
            else if (op == "subtract") r["matrix"] = matrix_json(A - B);
            else r["matrix"] = matrix_json(A.hadamard(B));
        } else if (op == "trace") r["value"] = num(A.trace());
        else if (op == "norms") {
            r["frobenius"] = num(A.norm_frobenius());
            r["norm1"] = num(A.norm_1());
            r["normInf"] = num(A.norm_inf());
        } else if (op == "power") {
            const int n = p.integer("n", 2, 0, 1000);
            r["matrix"] = matrix_json(A.pow(static_cast<unsigned>(n)));
        } else if (op == "cofactor") {
            if (A.rows() > 12) throw EngineError("LIMIT_EXCEEDED", "cofactor matrix is limited to 12x12");
            r["matrix"] = matrix_json(A.cofactor_matrix());
        } else if (op == "adjugate") {
            if (A.rows() > 12) throw EngineError("LIMIT_EXCEEDED", "adjugate is limited to 12x12");
            r["matrix"] = matrix_json(A.adjugate());
        } else if (op == "condition") r["value"] = num(A.condition_number());
        else if (op == "scale") r["matrix"] = matrix_json(A * p.finite("k", 1.0));
        else if (op == "power_iteration") {
            const auto pi = A.power_iteration(p.integer("maxIter", 1000, 1, LIMITS.max_iterations), p.positive("tol", 1e-12));
            r["value"] = num(pi.first);
            r["vector"] = Json(pi.second);
        } else if (op == "info") {
            r["square"] = A.is_square();
            r["symmetric"] = A.is_symmetric();
            if (A.is_square()) {
                r["determinant"] = num(A.determinant());
                r["trace"] = num(A.trace());
            }
            r["rank"] = static_cast<double>(A.rank());
            r["frobenius"] = num(A.norm_frobenius());
        }
    } catch (const std::invalid_argument& e) {
        throw EngineError("INVALID_PARAM", e.what(), "A");
    } catch (const std::runtime_error& e) {
        throw EngineError("NUMERIC_ERROR", e.what());
    } catch (const std::length_error& e) {
        throw EngineError("LIMIT_EXCEEDED", e.what());
    } catch (const std::out_of_range& e) {
        throw EngineError("INVALID_PARAM", e.what(), "A");
    }
    return r;
}

// optimize -------------------------------------------------------------------
const std::vector<std::string> MIN_METHODS = {"golden", "fibonacci", "brent", "newton", "binary", "linear_search", "variable_step"};

Json op_optimize_minimize(const Params& p) {
    const std::string method = p.choice("method", "brent", MIN_METHODS);
    ScalarFn f = scalar_function(p, "f");
    const bool maximize = p.boolean("maximize", false);
    std::function<double(double)> fn = f.fn;
    if (maximize) { auto g = f.fn; fn = [g](double x) { return -g(x); }; }
    const MinOptions opt = min_options(p);
    MinResult r;
    Json extra = Json::object();
    if (method == "newton") {
        const double x0 = p.finite("x0");
        try {
            Expression d1 = f.expr.derivative(f.var);
            Expression d2 = d1.derivative(f.var);
            extra["derivative"] = d1.to_string();
            extra["secondDerivative"] = d2.to_string();
            auto d1f = d1.as_function(f.var), d2f = d2.as_function(f.var);
            if (maximize) {
                RealFn nd1 = [d1f](double x) { return -d1f(x); };
                RealFn nd2 = [d2f](double x) { return -d2f(x); };
                r = newton_minimize(fn, nd1, nd2, x0, opt);
            } else {
                r = newton_minimize(fn, d1f, d2f, x0, opt);
            }
        } catch (const ExpressionError&) {
            r = newton_minimize(fn, x0, opt);
        }
    } else {
        const double a = p.finite("a"), b = p.finite("b");
        if (method == "golden") r = golden_section(fn, a, b, opt);
        else if (method == "fibonacci") r = fibonacci_search(fn, a, b, opt);
        else if (method == "brent") r = brent_minimize(fn, a, b, opt);
        else if (method == "binary") r = binary_search_min(fn, a, b, opt);
        else if (method == "linear_search") r = linear_search_min(fn, a, b, p.positive("step", abs(b - a) / 100.0), opt);
        else r = variable_step_min(fn, a, b, p.positive("step", abs(b - a) / 100.0), opt);
    }
    if (maximize) {
        r.fx = -r.fx;
        for (MinStep& s : r.history) s.fx = -s.fx;
    }
    Json j = min_json(r);
    j["expression"] = f.expr.to_string();
    j["var"] = f.var;
    j["maximize"] = maximize;
    for (const auto& kv : extra.as_object()) j[kv.first] = kv.second;
    return j;
}

Json op_optimize_bracket(const Params& p) {
    ScalarFn f = scalar_function(p, "f");
    const Bracket b = bracket_minimum(f.fn, p.finite("x0", 0.0), p.positive("step", 0.1), p.integer("maxIter", 200, 1, 10000));
    Json r = Json::object();
    r["a"] = num(b.a); r["b"] = num(b.b); r["c"] = num(b.c);
    r["fa"] = num(b.fa); r["fb"] = num(b.fb); r["fc"] = num(b.fc);
    r["evaluations"] = b.evaluations;
    r["found"] = b.found;
    return r;
}

Json op_optimize_minimize_nd(const Params& p) {
    const std::string method = p.choice("method", "nelder_mead", {"nelder_mead", "gradient_descent"});
    Expression e = p.expression("f");
    std::vector<std::string> vars = p.strings("vars", 64);
    if (vars.empty()) vars = e.variables();
    if (vars.empty()) invalid("f", "expression has no variables");
    for (const auto& v : e.variables()) {
        bool ok = false;
        for (const auto& w : vars) if (w == v) ok = true;
        if (!ok) invalid("vars", "expression uses variable '" + v + "' which is not listed in 'vars'");
    }
    std::vector<double> x0 = p.numbers("x0", 64, 1);
    if (x0.size() != vars.size()) invalid("x0", "'x0' must have one value per variable (" + std::to_string(vars.size()) + ")");
    const bool maximize = p.boolean("maximize", false);
    std::function<double(const std::vector<double>&)> fn;
    try {
        auto raw = e.as_function_n(vars);
        fn = maximize ? VecFn([raw](const std::vector<double>& v) { return -raw(v); }) : VecFn(raw);
    } catch (const ExpressionError& ex) {
        throw EngineError("EXPRESSION_ERROR", ex.what(), "f");
    }
    MinOptionsN opt;
    opt.tol = p.positive("tol", 1e-8);
    opt.max_iter = p.integer("maxIter", 2000, 1, LIMITS.max_iterations);
    opt.record_path = p.boolean("path", true);
    opt.max_path = p.integer("maxPath", 5000, 0, 100000);
    MinResultN r;
    try {
        if (method == "nelder_mead") r = nelder_mead(fn, x0, p.finite("step", 1.0), opt);
        else r = gradient_descent(fn, x0, p.positive("learningRate", 0.1), opt);
    } catch (const std::invalid_argument& ex) {
        throw EngineError("INVALID_PARAM", ex.what());
    }
    if (maximize) { r.fx = -r.fx; for (double& v : r.path_fx) v = -v; }
    Json j = minn_json(r);
    j["expression"] = e.to_string();
    j["vars"] = Json(vars);
    j["maximize"] = maximize;
    return j;
}

Json op_optimize_surface(const Params& p) {
    Expression e = p.expression("f");
    std::vector<std::string> vars = p.strings("vars", 2);
    if (vars.empty()) vars = e.variables();
    if (vars.size() != 2) invalid("vars", "surface needs exactly two variables");
    const double x0 = p.finite("xFrom", -5), x1 = p.finite("xTo", 5), y0 = p.finite("yFrom", -5), y1 = p.finite("yTo", 5);
    const int nx = p.integer("xSamples", 60, 2, 400), ny = p.integer("ySamples", 60, 2, 400);
    std::function<double(const std::vector<double>&)> fn;
    try { fn = e.as_function_n(vars); } catch (const ExpressionError& ex) { throw EngineError("EXPRESSION_ERROR", ex.what(), "f"); }
    Json xs = Json::array(), ys = Json::array(), z = Json::array();
    for (int i = 0; i < nx; ++i) xs.push_back(num(x0 + (x1 - x0) * i / (nx - 1)));
    for (int j = 0; j < ny; ++j) ys.push_back(num(y0 + (y1 - y0) * j / (ny - 1)));
    double zmin = inf(), zmax = -inf();
    for (int j = 0; j < ny; ++j) {
        Json row = Json::array();
        for (int i = 0; i < nx; ++i) {
            const double v = fn({xs.at(static_cast<std::size_t>(i)).as_number(), ys.at(static_cast<std::size_t>(j)).as_number()});
            if (is_finite(v)) { zmin = fmin(zmin, v); zmax = fmax(zmax, v); }
            row.push_back(num(v));
        }
        z.push_back(row);
    }
    Json r = Json::object();
    r["x"] = std::move(xs); r["y"] = std::move(ys); r["z"] = std::move(z);
    r["zMin"] = num(zmin); r["zMax"] = num(zmax);
    r["vars"] = Json(vars);
    return r;
}

// integrate ----------------------------------------------------------------
const std::vector<std::string> INT_METHODS = {"midpoint", "trapezoid", "simpson", "simpson38", "boole", "romberg",
                                              "gauss_legendre", "adaptive_simpson", "monte_carlo", "auto"};

IntegralResult run_integral(const std::string& method, const RealFn& f, double a, double b, const Params& p) {
    const int n = p.integer("n", 100, 1, LIMITS.max_integration_n);
    if (method == "midpoint") return midpoint(f, a, b, n);
    if (method == "trapezoid") return trapezoid(f, a, b, n);
    if (method == "simpson") return simpson(f, a, b, n);
    if (method == "simpson38") return simpson38(f, a, b, n);
    if (method == "boole") return boole(f, a, b, n);
    if (method == "romberg") return romberg(f, a, b, p.integer("maxLevels", 20, 1, 30), p.positive("tol", 1e-12));
    if (method == "gauss_legendre") return gauss_legendre(f, a, b, p.integer("points", 10, 1, 64));
    if (method == "adaptive_simpson") return adaptive_simpson(f, a, b, p.positive("tol", 1e-10), p.integer("maxDepth", 50, 1, 60));
    if (method == "monte_carlo") return monte_carlo(f, a, b, p.integer("samples", 100000, 1, 5000000), static_cast<unsigned long long>(p.number("seed", 42)));
    return integrate(f, a, b, p.positive("tol", 1e-10));
}

double bound(const Params& p, const char* name) {
    const double v = p.number(name);
    if (is_nan(v)) invalid(name, std::string("'") + name + "' must be a number");
    return v;
}

Json op_integrate_compute(const Params& p) {
    const std::string method = p.choice("method", "auto", INT_METHODS);
    ScalarFn f = scalar_function(p, "f");
    const double a = bound(p, "a"), b = bound(p, "b");
    if ((is_inf(a) || is_inf(b)) && method != "auto") invalid("a", "infinite bounds are only supported with method 'auto'");
    IntegralResult r;
    try { r = run_integral(method, f.fn, a, b, p); }
    catch (const std::invalid_argument& e) { throw EngineError("INVALID_PARAM", e.what()); }
    Json j = integral_json(r);
    j["expression"] = f.expr.to_string();
    j["var"] = f.var;
    j["a"] = num(a);
    j["b"] = num(b);
    return j;
}

Json op_integrate_compare(const Params& p) {
    ScalarFn f = scalar_function(p, "f");
    const double a = p.finite("a"), b = p.finite("b");
    const IntegralResult ref = integrate(f.fn, a, b, 1e-13);
    Json results = Json::array();
    for (const std::string& m : INT_METHODS) {
        if (m == "auto") continue;
        try {
            IntegralResult r = run_integral(m, f.fn, a, b, p);
            Json j = integral_json(r);
            j.as_object().erase(std::remove_if(j.as_object().begin(), j.as_object().end(),
                                               [](const std::pair<std::string, Json>& kv) { return kv.first == "table"; }),
                                j.as_object().end());
            j["absError"] = num(abs(r.value - ref.value));
            results.push_back(j);
        } catch (const std::exception& e) {
            Json j = Json::object();
            j["method"] = m;
            j["message"] = e.what();
            results.push_back(j);
        }
    }
    Json out = Json::object();
    out["expression"] = f.expr.to_string();
    out["reference"] = num(ref.value);
    out["results"] = results;
    return out;
}

Json op_integrate_convergence(const Params& p) {
    const std::string method = p.choice("method", "simpson", {"midpoint", "trapezoid", "simpson", "simpson38", "boole", "gauss_legendre"});
    ScalarFn f = scalar_function(p, "f");
    const double a = p.finite("a"), b = p.finite("b");
    const IntegralResult ref = integrate(f.fn, a, b, 1e-13);
    std::vector<double> ns = p.numbers("ns", 64);
    if (ns.empty()) ns = {2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
    Json rows = Json::array();
    for (double nd : ns) {
        if (!is_integer(nd) || nd < 1 || nd > LIMITS.max_integration_n) invalid("ns", "ns must be positive integers");
        const int n = static_cast<int>(nd);
        IntegralResult r;
        if (method == "midpoint") r = midpoint(f.fn, a, b, n);
        else if (method == "trapezoid") r = trapezoid(f.fn, a, b, n);
        else if (method == "simpson") r = simpson(f.fn, a, b, n);
        else if (method == "simpson38") r = simpson38(f.fn, a, b, n);
        else if (method == "boole") r = boole(f.fn, a, b, n);
        else r = gauss_legendre(f.fn, a, b, std::min(n, 64));
        Json row = Json::object();
        row["n"] = static_cast<double>(r.subdivisions);
        row["value"] = num(r.value);
        row["absError"] = num(abs(r.value - ref.value));
        rows.push_back(row);
    }
    Json out = Json::object();
    out["method"] = method;
    out["reference"] = num(ref.value);
    out["rows"] = rows;
    return out;
}

// ode ----------------------------------------------------------------------
Json op_ode_solve(const Params& p) {
    const std::string method = p.choice("method", "rk45", {"euler", "heun", "midpoint", "rk4", "rk45"});
    // Equations: either "f" (scalar, variables t and y) or "equations": [...] with "state": [...]
    std::vector<std::string> state = p.strings("state", 50);
    std::vector<std::string> eq_text;
    if (p.has("equations")) eq_text = p.strings("equations", 50);
    else eq_text.push_back(p.string("f"));
    if (eq_text.empty()) invalid("equations", "at least one equation is required");
    if (state.empty()) {
        if (eq_text.size() == 1) state = {"y"};
        else for (std::size_t i = 0; i < eq_text.size(); ++i) state.push_back("y" + std::to_string(i));
    }
    if (state.size() != eq_text.size()) invalid("state", "'state' must have one name per equation");
    const std::string tvar = p.string("tVar", "t");
    std::vector<std::string> names = state;
    names.push_back(tvar);
    std::vector<Expression::Compiled> compiled;
    std::vector<std::string> printed;
    for (std::size_t i = 0; i < eq_text.size(); ++i) {
        try {
            Expression e = Expression::parse(eq_text[i]);
            for (const auto& v : e.variables()) {
                bool ok = false;
                for (const auto& n : names) if (n == v) ok = true;
                if (!ok) invalid("equations", "equation " + std::to_string(i + 1) + " uses unknown variable '" + v + "'");
            }
            compiled.push_back(e.compile(names));
            printed.push_back(e.to_string());
        } catch (const ExpressionError& ex) {
            throw EngineError("EXPRESSION_ERROR", "equation " + std::to_string(i + 1) + ": " + ex.what(), "equations", static_cast<long>(ex.position()));
        }
    }
    const std::vector<double> y0 = p.numbers("y0", 50, 1);
    if (y0.size() != state.size()) invalid("y0", "'y0' must have one value per state variable (" + std::to_string(state.size()) + ")");
    const double t0 = p.finite("t0", 0.0), t1 = p.finite("t1", 10.0);
    OdeOptions opt;
    opt.tol = p.positive("tol", 1e-8);
    opt.max_steps = p.integer("maxSteps", 100000, 1, LIMITS.max_ode_steps);
    const double h = p.number("h", 0.0);
    if (method != "rk45" && !(h > 0.0)) invalid("h", "fixed-step methods need a positive step size 'h'");
    if (method != "rk45" && abs(t1 - t0) / h > opt.max_steps) invalid("h", "step size too small for the interval (more than maxSteps steps)");
    const std::size_t n = state.size();
    OdeFn f = [&](double t, const std::vector<double>& y) {
        std::vector<double> vals(n + 1);
        for (std::size_t i = 0; i < n; ++i) vals[i] = y[i];
        vals[n] = t;
        std::vector<double> dy(n);
        for (std::size_t i = 0; i < n; ++i) dy[i] = compiled[i](vals.data(), vals.size());
        return dy;
    };
    OdeResult r;
    try {
        if (method == "euler") r = euler(f, t0, y0, t1, h, opt);
        else if (method == "heun") r = heun(f, t0, y0, t1, h, opt);
        else if (method == "midpoint") r = midpoint_method(f, t0, y0, t1, h, opt);
        else if (method == "rk4") r = rk4(f, t0, y0, t1, h, opt);
        else r = rk45(f, t0, y0, t1, h, opt);
    } catch (const std::invalid_argument& e) {
        throw EngineError("INVALID_PARAM", e.what());
    }
    // Thin the output for the UI if it is very long.
    const int max_points = p.integer("maxPoints", 5000, 2, LIMITS.max_samples);
    Json t = Json::array();
    Json y = Json::array();  // y[state][point]
    std::vector<Json> cols(n, Json::array());
    const std::size_t total = r.t.size();
    const std::size_t stride = total > static_cast<std::size_t>(max_points) ? (total + max_points - 1) / max_points : 1;
    for (std::size_t i = 0; i < total; i += stride) {
        t.push_back(num(r.t[i]));
        for (std::size_t k = 0; k < n; ++k) cols[k].push_back(num(r.y[i][k]));
    }
    if (total > 0 && (total - 1) % stride != 0) {
        t.push_back(num(r.t[total - 1]));
        for (std::size_t k = 0; k < n; ++k) cols[k].push_back(num(r.y[total - 1][k]));
    }
    for (std::size_t k = 0; k < n; ++k) y.push_back(std::move(cols[k]));
    Json j = Json::object();
    j["method"] = r.method;
    j["state"] = Json(state);
    j["equations"] = Json(printed);
    j["t"] = std::move(t);
    j["y"] = std::move(y);
    j["final"] = Json(r.y.empty() ? std::vector<double>() : r.y.back());
    j["steps"] = r.steps;
    j["rejected"] = r.rejected;
    j["evaluations"] = r.evaluations;
    j["success"] = r.success;
    j["message"] = r.message;
    j["thinned"] = stride > 1;
    return j;
}

// interpolate ----------------------------------------------------------------
Json op_interpolate_compute(const Params& p) {
    const std::string method = p.choice("method", "spline", {"lagrange", "newton", "linear", "spline", "spline_clamped", "polyfit"});
    std::vector<double> xs, ys;
    if (!p.has("points")) {
        xs = p.numbers("xs", 100000, 1);
        ys = p.numbers("ys", 100000, 1);
        if (xs.size() != ys.size()) invalid("ys", "'xs' and 'ys' must have the same length");
    } else {
        // alternative: points: [[x, y], ...]
        const Json& pts = p.raw("points");
        if (!pts.is_array()) invalid("points", "'points' must be an array of [x, y] pairs");
        xs.clear(); ys.clear();
        for (const Json& pt : pts.as_array()) {
            if (!pt.is_array() || pt.size() != 2 || !pt.at(0).is_number() || !pt.at(1).is_number())
                invalid("points", "'points' must be an array of [x, y] number pairs");
            xs.push_back(pt.at(0).as_number());
            ys.push_back(pt.at(1).as_number());
            if (xs.size() > 100000) invalid("points", "too many points (limit 100000)");
        }
        for (std::size_t i = 0; i < xs.size(); ++i)
            if (!is_finite(xs[i]) || !is_finite(ys[i])) invalid("points", "points must be finite");
    }
    if (xs.empty()) invalid("xs", "at least one point is required");
    Json r = Json::object();
    r["method"] = method;
    r["count"] = static_cast<double>(xs.size());
    const bool want_curve = p.boolean("curve", true);
    double lo = *std::min_element(xs.begin(), xs.end()), hi = *std::max_element(xs.begin(), xs.end());
    const double pad = (hi - lo) * 0.05;
    const double from = p.finite("from", lo - pad), to = p.finite("to", hi + pad);
    const int samples = p.integer("samples", 400, 2, LIMITS.max_samples);
    std::function<double(double)> fn;
    try {
        if (method == "lagrange") {
            if (xs.size() > 200) invalid("xs", "lagrange is limited to 200 points");
            Polynomial poly = lagrange_polynomial(xs, ys);
            r["polynomial"] = poly_json(poly);
            fn = [poly](double x) { return poly.evaluate(x); };
        } else if (method == "newton") {
            if (xs.size() > 200) invalid("xs", "newton is limited to 200 points");
            NewtonInterpolation nd = newton_divided_differences(xs, ys);
            r["polynomial"] = poly_json(nd.polynomial);
            r["coefficients"] = Json(nd.coefficients);
            r["table"] = Json(nd.table);
            fn = [nd](double x) { return nd.evaluate(x); };
        } else if (method == "linear") {
            std::vector<double> sx = xs, sy = ys;
            sort_points(sx, sy);
            fn = [sx, sy](double x) { return linear_interpolate(sx, sy, x); };
        } else if (method == "spline" || method == "spline_clamped") {
            CubicSpline s = method == "spline" ? CubicSpline::natural(xs, ys)
                                               : CubicSpline::clamped(xs, ys, p.finite("d0", 0.0), p.finite("dn", 0.0));
            Json segs = Json::array();
            for (const auto& g : s.segments()) {
                Json sj = Json::object();
                sj["x0"] = num(g.x0); sj["x1"] = num(g.x1);
                sj["a"] = num(g.a); sj["b"] = num(g.b); sj["c"] = num(g.c); sj["d"] = num(g.d);
                segs.push_back(sj);
            }
            r["segments"] = segs;
            fn = [s](double x) { return s(x); };
        } else {
            const int degree = p.integer("degree", 2, 0, 50);
            FitResult fit = polyfit(xs, ys, degree);
            r["polynomial"] = poly_json(fit.polynomial);
            r["rSquared"] = num(fit.r_squared);
            r["rmse"] = num(fit.rmse);
            r["residuals"] = Json(fit.residuals);
            Polynomial poly = fit.polynomial;
            fn = [poly](double x) { return poly.evaluate(x); };
        }
    } catch (const std::invalid_argument& e) {
        throw EngineError("INVALID_PARAM", e.what(), "xs");
    } catch (const std::runtime_error& e) {
        throw EngineError("NUMERIC_ERROR", e.what());
    }
    if (p.has("x")) {
        const double x = p.finite("x");
        r["x"] = num(x);
        r["value"] = num(fn(x));
    }
    if (p.has("xq")) {
        const std::vector<double> xq = p.numbers("xq", LIMITS.max_samples);
        Json vals = Json::array();
        for (double x : xq) vals.push_back(num(fn(x)));
        r["values"] = vals;
    }
    if (want_curve) r["curve"] = table_json(fn, from, to, samples);
    return r;
}

// stats ----------------------------------------------------------------------
Json op_stats_describe(const Params& p) {
    const std::vector<double> data = p.numbers("data", LIMITS.max_data_points, 1);
    Descriptive s;
    try { s = describe(data); } catch (const std::invalid_argument& e) { throw EngineError("INVALID_PARAM", e.what(), "data"); }
    Json r = Json::object();
    r["count"] = static_cast<double>(s.count);
    r["sum"] = num(s.sum); r["mean"] = num(s.mean); r["median"] = num(s.median); r["mode"] = num(s.mode);
    r["min"] = num(s.min); r["max"] = num(s.max); r["range"] = num(s.range);
    r["variance"] = num(s.variance); r["variancePopulation"] = num(s.variance_population);
    r["stddev"] = num(s.stddev); r["stddevPopulation"] = num(s.stddev_population); r["stdError"] = num(s.std_error);
    r["q1"] = num(s.q1); r["q3"] = num(s.q3); r["iqr"] = num(s.iqr);
    r["skewness"] = num(s.skewness); r["kurtosis"] = num(s.kurtosis);
    r["geometricMean"] = num(s.geometric_mean); r["harmonicMean"] = num(s.harmonic_mean);
    r["meanAbsDev"] = num(s.mean_abs_dev); r["coefficientOfVariation"] = num(s.coefficient_of_variation);
    const std::size_t bins = static_cast<std::size_t>(p.integer("bins", 0, 0, 10000));
    const Histogram h = histogram(data, bins);
    Json hj = Json::object();
    hj["edges"] = Json(h.edges);
    Json counts = Json::array();
    for (auto c : h.counts) counts.push_back(static_cast<double>(c));
    hj["counts"] = counts;
    hj["density"] = Json(h.density);
    r["histogram"] = hj;
    std::vector<double> sorted = data;
    std::sort(sorted.begin(), sorted.end());
    if (sorted.size() <= 5000) r["sorted"] = Json(sorted);
    Json quant = Json::object();
    for (double q : {0.01, 0.05, 0.1, 0.25, 0.5, 0.75, 0.9, 0.95, 0.99}) quant[to_string(q, 3)] = num(quantile(data, q));
    r["quantiles"] = quant;
    return r;
}

Json op_stats_regression(const Params& p) {
    const std::vector<double> x = p.numbers("x", LIMITS.max_data_points, 2), y = p.numbers("y", LIMITS.max_data_points, 2);
    Json r = Json::object();
    try {
        const LinearRegression lr = linear_regression(x, y);
        r["slope"] = num(lr.slope); r["intercept"] = num(lr.intercept);
        r["r"] = num(lr.r); r["rSquared"] = num(lr.r_squared);
        r["stdErrorSlope"] = num(lr.std_error_slope); r["stdErrorIntercept"] = num(lr.std_error_intercept);
        r["rmse"] = num(lr.rmse);
        r["residuals"] = Json(lr.residuals);
        r["equation"] = "y = " + to_string(lr.slope, 8) + "x " + (lr.intercept < 0 ? "- " : "+ ") + to_string(abs(lr.intercept), 8);
        r["pearson"] = num(correlation(x, y));
        r["spearman"] = num(spearman(x, y));
        r["covariance"] = num(covariance(x, y));
        const int degree = p.integer("degree", 1, 1, 20);
        if (degree > 1) {
            const FitResult fit = polyfit(x, y, degree);
            r["polynomial"] = poly_json(fit.polynomial);
            r["polynomialRSquared"] = num(fit.r_squared);
        }
    } catch (const std::invalid_argument& e) {
        throw EngineError("INVALID_PARAM", e.what(), "x");
    } catch (const std::runtime_error& e) {
        throw EngineError("NUMERIC_ERROR", e.what());
    }
    return r;
}

Json op_stats_transform(const Params& p) {
    const std::string op = p.choice("fn", "zscore", {"zscore", "moving_average", "cumsum", "normalize", "ranks", "sort"});
    const std::vector<double> data = p.numbers("data", LIMITS.max_data_points, 1);
    Json r = Json::object();
    r["fn"] = op;
    try {
        if (op == "zscore") r["result"] = Json(z_scores(data));
        else if (op == "moving_average") r["result"] = Json(moving_average(data, static_cast<std::size_t>(p.integer("window", 3, 1, 1000000))));
        else if (op == "cumsum") r["result"] = Json(cumulative_sum(data));
        else if (op == "normalize") r["result"] = Json(min_max_normalize(data));
        else if (op == "ranks") r["result"] = Json(ranks(data));
        else { std::vector<double> s = data; std::sort(s.begin(), s.end()); r["result"] = Json(s); }
    } catch (const std::invalid_argument& e) {
        throw EngineError("INVALID_PARAM", e.what(), "data");
    }
    return r;
}

// ---------------------------------------------------------------------------
// Registry construction
// ---------------------------------------------------------------------------
const Registry& registry() {
    static const Registry reg = [] {
        Registry r;
        r.add("meta.ops", "List every operation with its parameter schema.", P({}), op_meta_ops);
        r.add("meta.version", "Engine version and limits.", P({}), [](const Params&) {
            Json j = Json::object();
            j["version"] = version();
            Json l = Json::object();
            l["maxIterations"] = LIMITS.max_iterations;
            l["maxSamples"] = LIMITS.max_samples;
            l["maxIntegrationN"] = LIMITS.max_integration_n;
            l["maxOdeSteps"] = LIMITS.max_ode_steps;
            l["maxMatrixDim"] = static_cast<double>(LIMITS.max_matrix_dim);
            l["maxPolynomialDegree"] = LIMITS.max_polynomial_degree;
            l["maxDataPoints"] = static_cast<double>(LIMITS.max_data_points);
            l["maxRequestBytes"] = static_cast<double>(LIMITS.max_request_bytes);
            j["limits"] = l;
            return j;
        });
        r.add("expression.evaluate", "Evaluate an expression with optional variable bindings.",
              P({{"expr*", "string: expression such as sin(x)^2 + 3x"}, {"x", "number"}, {"y", "number"}, {"t", "number"}, {"vars", "object: name -> number"}}),
              op_expression_evaluate);
        r.add("expression.parse", "Parse, normalise and report the variables of an expression.", P({{"expr*", "string"}}), op_expression_parse);
        r.add("expression.derivative", "Symbolic derivative (optionally evaluated at a point).",
              P({{"expr*", "string"}, {"var", "string: default x"}, {"order", "integer 1..6"}, {"at", "number"}}), op_expression_derivative);
        r.add("expression.table", "Sample an expression over an interval for plotting.",
              P({{"expr*", "string"}, {"var", "string"}, {"from", "number"}, {"to", "number"}, {"samples", "integer"}, {"derivative", "boolean: also sample f'"}}),
              op_expression_table);
        r.add("expression.functions", "Built-in function and constant names.", P({}), op_expression_functions);
        r.add("core.evaluate", "Evaluate one from-scratch elementary function.", P({{"fn*", "string"}, {"x*", "number"}}), op_core_evaluate);
        r.add("core.compare", "Compare a cml function against the platform libm over an interval (ULP error).",
              P({{"fn*", "string"}, {"from", "number"}, {"to", "number"}, {"samples", "integer"}}), op_core_compare);
        r.add("core.constants", "Mathematical constants.", P({}), op_core_constants);
        r.add("core.functions", "Names of the elementary functions available to core.evaluate.", P({}), op_core_functions);
        r.add("special.numberTheory", "Factorials, binomials, gcd/lcm, primes, Fibonacci, beta.",
              P({{"fn*", "factorial|binomial|permutations|gcd|lcm|isPrime|primeFactors|primes|fibonacci|beta"}, {"n", "number"}, {"k", "number"}, {"a", "number"}, {"b", "number"}}),
              op_special_number_theory);
        r.add("roots.find", "Find a root of f(x) = 0 with the chosen method; returns the iteration history.",
              P({{"f*", "string"}, {"method", "bisection|regula_falsi|secant|newton|halley|brent|ridders|steffensen|fixed_point|linear_search|variable_step"},
                 {"a", "number: bracket low"}, {"b", "number: bracket high"}, {"x0", "number: initial guess"}, {"x1", "number: second guess (secant)"},
                 {"df", "string: derivative (optional, symbolic by default)"}, {"step", "number"}, {"tol", "number"}, {"maxIter", "integer"}, {"history", "boolean"}}),
              op_roots_find);
        r.add("roots.compare", "Run every applicable root-finding method and compare.",
              P({{"f*", "string"}, {"a", "number"}, {"b", "number"}, {"x0", "number"}, {"tol", "number"}, {"maxIter", "integer"}}), op_roots_compare);
        r.add("roots.all", "Scan an interval and refine every sign change.",
              P({{"f*", "string"}, {"a*", "number"}, {"b*", "number"}, {"samples", "integer"}, {"tol", "number"}}), op_roots_all);
        r.add("polynomial.parse", "Parse a polynomial from text or coefficients.", P({{"p*", "string | number[]"}}), op_polynomial_parse);
        r.add("polynomial.evaluate", "Evaluate a polynomial (and its derivative) at x.", P({{"p*", "string | number[]"}, {"x", "number"}, {"xs", "number[]"}}), op_polynomial_evaluate);
        r.add("polynomial.roots", "All complex roots (Aberth–Ehrlich) plus the real ones.", P({{"p*", "string | number[]"}, {"tol", "number"}, {"maxIter", "integer"}}), op_polynomial_roots);
        r.add("polynomial.calculus", "Derivative, antiderivative or definite integral.",
              P({{"p*", "string | number[]"}, {"fn", "derivative|integral|definite_integral"}, {"order", "integer"}, {"constant", "number"}, {"a", "number"}, {"b", "number"}}),
              op_polynomial_calculus);
        r.add("polynomial.arithmetic", "Add, subtract, multiply, divide, gcd, compose, power, scale.",
              P({{"a*", "string | number[]"}, {"b", "string | number[]"}, {"fn", "add|subtract|multiply|divide|modulo|gcd|compose|power|scale"}, {"n", "integer"}, {"k", "number"}}),
              op_polynomial_arithmetic);
        r.add("polynomial.table", "Sample a polynomial for plotting.", P({{"p*", "string | number[]"}, {"from", "number"}, {"to", "number"}, {"samples", "integer"}}), op_polynomial_table);
        r.add("polynomial.fromRoots", "Build a polynomial from its roots.", P({{"roots*", "number[]"}, {"leading", "number"}}), op_polynomial_from_roots);
        r.add("matrix.compute", "Linear algebra on a matrix A (and optionally B / b).",
              P({{"A*", "number[][] | string"}, {"B", "number[][]"}, {"b", "number[]"},
                 {"fn", "determinant|inverse|transpose|rank|rref|lu|qr|cholesky|eigen|solve|least_squares|multiply|add|subtract|hadamard|trace|norms|power|cofactor|adjugate|condition|scale|info|power_iteration"},
                 {"n", "integer: power"}, {"k", "number: scale"}, {"tol", "number"}, {"strassen", "boolean"}}),
              op_matrix_compute);
        r.add("optimize.minimize", "One-dimensional minimisation (or maximisation) with iteration history.",
              P({{"f*", "string"}, {"method", "golden|fibonacci|brent|newton|binary|linear_search|variable_step"}, {"a", "number"}, {"b", "number"},
                 {"x0", "number: newton"}, {"step", "number"}, {"tol", "number"}, {"maxIter", "integer"}, {"maximize", "boolean"}}),
              op_optimize_minimize);
        r.add("optimize.bracket", "Expand from x0 to bracket a minimum.", P({{"f*", "string"}, {"x0", "number"}, {"step", "number"}}), op_optimize_bracket);
        r.add("optimize.minimizeNd", "Nelder–Mead or gradient descent in N dimensions with the search path.",
              P({{"f*", "string: e.g. (1-x)^2 + 100(y-x^2)^2"}, {"method", "nelder_mead|gradient_descent"}, {"vars", "string[]"}, {"x0*", "number[]"},
                 {"step", "number"}, {"learningRate", "number"}, {"tol", "number"}, {"maxIter", "integer"}, {"maximize", "boolean"}}),
              op_optimize_minimize_nd);
        r.add("optimize.surface", "Sample f(x, y) on a grid for contour/heatmap plots.",
              P({{"f*", "string"}, {"vars", "string[2]"}, {"xFrom", "number"}, {"xTo", "number"}, {"yFrom", "number"}, {"yTo", "number"}, {"xSamples", "integer"}, {"ySamples", "integer"}}),
              op_optimize_surface);
        r.add("integrate.compute", "Definite integral with the chosen rule (auto handles infinite bounds).",
              P({{"f*", "string"}, {"a*", "number"}, {"b*", "number"}, {"method", "midpoint|trapezoid|simpson|simpson38|boole|romberg|gauss_legendre|adaptive_simpson|monte_carlo|auto"},
                 {"n", "integer"}, {"points", "integer"}, {"tol", "number"}, {"maxLevels", "integer"}, {"samples", "integer"}, {"seed", "number"}}),
              op_integrate_compute);
        r.add("integrate.compare", "Run every rule with the same settings and compare against a high-precision reference.",
              P({{"f*", "string"}, {"a*", "number"}, {"b*", "number"}, {"n", "integer"}, {"points", "integer"}}), op_integrate_compare);
        r.add("integrate.convergence", "Error of a rule as the subdivision count grows.",
              P({{"f*", "string"}, {"a*", "number"}, {"b*", "number"}, {"method", "midpoint|trapezoid|simpson|simpson38|boole|gauss_legendre"}, {"ns", "integer[]"}}),
              op_integrate_convergence);
        r.add("ode.solve", "Solve y' = f(t, y) (scalar or system) with Euler/Heun/midpoint/RK4/RK45.",
              P({{"f", "string: dy/dt in terms of t and y"}, {"equations", "string[]: one per state variable"}, {"state", "string[]: state names"},
                 {"y0*", "number[]"}, {"t0", "number"}, {"t1", "number"}, {"h", "number: step (fixed-step methods)"}, {"method", "euler|heun|midpoint|rk4|rk45"},
                 {"tol", "number"}, {"maxSteps", "integer"}, {"maxPoints", "integer"}}),
              op_ode_solve);
        r.add("interpolate.compute", "Interpolate or fit points; returns the curve for plotting.",
              P({{"xs", "number[]"}, {"ys", "number[]"}, {"points", "[x, y][]"}, {"method", "lagrange|newton|linear|spline|spline_clamped|polyfit"},
                 {"degree", "integer: polyfit"}, {"x", "number: evaluate here"}, {"xq", "number[]"}, {"from", "number"}, {"to", "number"}, {"samples", "integer"}, {"d0", "number"}, {"dn", "number"}}),
              op_interpolate_compute);
        r.add("stats.describe", "Descriptive statistics, quantiles and a histogram.", P({{"data*", "number[] | string"}, {"bins", "integer"}}), op_stats_describe);
        r.add("stats.regression", "Linear regression, correlation coefficients and optional polynomial fit.",
              P({{"x*", "number[]"}, {"y*", "number[]"}, {"degree", "integer"}}), op_stats_regression);
        r.add("stats.transform", "z-scores, moving average, cumulative sum, normalisation, ranks, sort.",
              P({{"data*", "number[]"}, {"fn", "zscore|moving_average|cumsum|normalize|ranks|sort"}, {"window", "integer"}}), op_stats_transform);
        return r;
    }();
    return reg;
}

Json op_meta_ops(const Params&) {
    Json arr = Json::array();
    for (const OpInfo& info : registry().infos) {
        Json j = Json::object();
        j["name"] = info.name;
        j["category"] = info.category;
        j["description"] = info.description;
        j["params"] = info.params;
        arr.push_back(j);
    }
    Json out = Json::object();
    out["ops"] = arr;
    out["count"] = static_cast<double>(registry().infos.size());
    return out;
}

Json error_response(const std::string& op, const Json& id, const std::string& code, const std::string& message,
                    const std::string& param = "", long position = -1) {
    Json r = Json::object();
    r["ok"] = false;
    if (!op.empty()) r["op"] = op;
    if (!id.is_null()) r["id"] = id;
    Json e = Json::object();
    e["code"] = code;
    e["message"] = message;
    if (!param.empty()) e["param"] = param;
    if (position >= 0) e["position"] = static_cast<double>(position);
    r["error"] = e;
    return r;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
const char* version() { return "2.0.0"; }
const Limits& limits() { return LIMITS; }
const std::vector<OpInfo>& ops() { return registry().infos; }
bool has_op(const std::string& name) { return registry().handlers.count(name) != 0; }

Json execute(const Json& request) {
    const auto start = std::chrono::steady_clock::now();
    std::string op;
    Json id;
    try {
        if (!request.is_object()) return error_response("", id, "INVALID_REQUEST", "request must be a JSON object with an 'op' field");
        const Json* idp = request.find("id");
        if (idp) id = *idp;
        const Json* opp = request.find("op");
        if (!opp || !opp->is_string()) return error_response("", id, "INVALID_REQUEST", "missing string field 'op'");
        op = opp->as_string();
        const auto it = registry().handlers.find(op);
        if (it == registry().handlers.end()) return error_response(op, id, "UNKNOWN_OP", "unknown op '" + op + "'; call meta.ops for the list");
        const Json* pp = request.find("params");
        Json empty = Json::object();
        const Json& params = (pp && !pp->is_null()) ? *pp : empty;
        if (!params.is_object()) return error_response(op, id, "INVALID_REQUEST", "'params' must be an object");
        Params p(params);
        Json result = it->second(p);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        Json r = Json::object();
        r["ok"] = true;
        r["op"] = op;
        if (!id.is_null()) r["id"] = id;
        r["result"] = std::move(result);
        r["timeMs"] = ms;
        return r;
    } catch (const EngineError& e) {
        return error_response(op, id, e.code, e.what(), e.param, e.position);
    } catch (const ExpressionError& e) {
        return error_response(op, id, "EXPRESSION_ERROR", e.what(), "", static_cast<long>(e.position()));
    } catch (const JsonError& e) {
        return error_response(op, id, "INVALID_PARAM", e.what());
    } catch (const std::invalid_argument& e) {
        return error_response(op, id, "INVALID_PARAM", e.what());
    } catch (const std::length_error& e) {
        return error_response(op, id, "LIMIT_EXCEEDED", e.what());
    } catch (const std::out_of_range& e) {
        return error_response(op, id, "INVALID_PARAM", e.what());
    } catch (const std::bad_alloc&) {
        return error_response(op, id, "LIMIT_EXCEEDED", "out of memory");
    } catch (const std::exception& e) {
        return error_response(op, id, "INTERNAL", e.what());
    } catch (...) {
        return error_response(op, id, "INTERNAL", "unknown error");
    }
}

std::string execute(const std::string& request_json) {
    Json req;
    try {
        req = Json::parse(request_json, 64, LIMITS.max_request_bytes);
    } catch (const JsonError& e) {
        return error_response("", Json(), "PARSE_ERROR", e.what()).dump();
    } catch (const std::exception& e) {
        return error_response("", Json(), "PARSE_ERROR", e.what()).dump();
    }
    try {
        return execute(req).dump();
    } catch (...) {
        return error_response("", Json(), "INTERNAL", "failed to serialise response").dump();
    }
}

} // namespace engine
} // namespace cml
