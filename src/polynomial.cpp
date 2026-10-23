// cml/polynomial.cpp — dense univariate polynomials.
#include "cml/polynomial.hpp"
#include "cml/core.hpp"

#include <algorithm>
#include <cstdlib>
#include <ostream>
#include <sstream>
#include <stdexcept>

namespace cml {

namespace {
double cabs(const Polynomial::Complex& z) { return hypot(z.real(), z.imag()); }
}

Polynomial::Polynomial() {}
Polynomial::Polynomial(std::vector<double> coefficients) : c_(std::move(coefficients)) { trim_inplace(); }
Polynomial::Polynomial(std::initializer_list<double> coefficients) : c_(coefficients) { trim_inplace(); }

Polynomial Polynomial::from_roots(const std::vector<double>& roots, double leading) {
    Polynomial p({leading});
    for (double r : roots) p *= Polynomial({-r, 1.0});
    return p;
}
Polynomial Polynomial::monomial(int degree, double coefficient) {
    if (degree < 0) throw std::invalid_argument("monomial degree must be >= 0");
    std::vector<double> c(static_cast<std::size_t>(degree) + 1, 0.0);
    c.back() = coefficient;
    return Polynomial(std::move(c));
}

void Polynomial::trim_inplace(double epsilon) {
    while (!c_.empty() && (c_.back() == 0.0 || abs(c_.back()) <= epsilon)) c_.pop_back();
}
Polynomial Polynomial::trimmed(double epsilon) const {
    Polynomial p = *this;
    p.trim_inplace(epsilon);
    return p;
}

Polynomial Polynomial::cleaned(double rel_eps) const {
    double scale = 0.0;
    for (double v : c_) scale = fmax(scale, abs(v));
    Polynomial p = *this;
    for (double& v : p.c_)
        if (abs(v) <= rel_eps * scale) v = 0.0;
    p.trim_inplace();
    return p;
}

int Polynomial::degree() const { return static_cast<int>(c_.size()) - 1; }
bool Polynomial::is_zero() const { return c_.empty(); }
bool Polynomial::is_monic() const { return !c_.empty() && c_.back() == 1.0; }
double Polynomial::leading() const { return c_.empty() ? 0.0 : c_.back(); }
double Polynomial::coefficient(int exponent) const {
    if (exponent < 0 || static_cast<std::size_t>(exponent) >= c_.size()) return 0.0;
    return c_[static_cast<std::size_t>(exponent)];
}
void Polynomial::set(int exponent, double value) {
    if (exponent < 0) throw std::invalid_argument("exponent must be >= 0");
    if (exponent > 100000) throw std::length_error("polynomial degree too large");
    if (static_cast<std::size_t>(exponent) >= c_.size()) c_.resize(static_cast<std::size_t>(exponent) + 1, 0.0);
    c_[static_cast<std::size_t>(exponent)] = value;
    trim_inplace();
}

double Polynomial::evaluate(double x) const {
    double r = 0.0;
    for (std::size_t i = c_.size(); i-- > 0;) r = r * x + c_[i];
    return r;
}
Polynomial::Complex Polynomial::evaluate(Complex z) const {
    Complex r(0.0, 0.0);
    for (std::size_t i = c_.size(); i-- > 0;) r = r * z + c_[i];
    return r;
}
std::pair<double, double> Polynomial::evaluate_with_derivative(double x) const {
    double p = 0.0, dp = 0.0;
    for (std::size_t i = c_.size(); i-- > 0;) {
        dp = dp * x + p;
        p = p * x + c_[i];
    }
    return {p, dp};
}

Polynomial Polynomial::derivative() const {
    if (c_.size() <= 1) return Polynomial();
    std::vector<double> d(c_.size() - 1);
    for (std::size_t i = 1; i < c_.size(); ++i) d[i - 1] = c_[i] * static_cast<double>(i);
    return Polynomial(std::move(d));
}
Polynomial Polynomial::integral(double constant) const {
    std::vector<double> r(c_.size() + 1);
    r[0] = constant;
    for (std::size_t i = 0; i < c_.size(); ++i) r[i + 1] = c_[i] / static_cast<double>(i + 1);
    return Polynomial(std::move(r));
}
double Polynomial::definite_integral(double a, double b) const {
    const Polynomial F = integral();
    return F.evaluate(b) - F.evaluate(a);
}
Polynomial Polynomial::compose(const Polynomial& inner) const {
    Polynomial r;
    for (std::size_t i = c_.size(); i-- > 0;) r = r * inner + Polynomial({c_[i]});
    return r;
}
Polynomial Polynomial::pow(unsigned n) const {
    if (n > 10000) throw std::length_error("polynomial power too large");
    Polynomial result({1.0});
    Polynomial base = *this;
    while (n) {
        if (n & 1) result *= base;
        n >>= 1;
        if (n) base *= base;
    }
    return result;
}
Polynomial Polynomial::normalized() const {
    if (is_zero()) return *this;
    return *this / leading();
}
Polynomial Polynomial::deflate(double root) const {
    if (c_.size() <= 1) return Polynomial();
    std::vector<double> q(c_.size() - 1);
    double carry = c_.back();
    for (std::size_t i = c_.size() - 1; i-- > 0;) {
        q[i] = carry;
        carry = c_[i] + carry * root;
    }
    return Polynomial(std::move(q));
}
Polynomial Polynomial::shifted(double h) const { return compose(Polynomial({h, 1.0})); }

Polynomial Polynomial::operator-() const {
    Polynomial r = *this;
    for (double& v : r.c_) v = -v;
    return r;
}
Polynomial& Polynomial::operator+=(const Polynomial& o) {
    if (o.c_.size() > c_.size()) c_.resize(o.c_.size(), 0.0);
    for (std::size_t i = 0; i < o.c_.size(); ++i) c_[i] += o.c_[i];
    trim_inplace();
    return *this;
}
Polynomial& Polynomial::operator-=(const Polynomial& o) {
    if (o.c_.size() > c_.size()) c_.resize(o.c_.size(), 0.0);
    for (std::size_t i = 0; i < o.c_.size(); ++i) c_[i] -= o.c_[i];
    trim_inplace();
    return *this;
}
Polynomial& Polynomial::operator*=(const Polynomial& o) {
    if (c_.empty() || o.c_.empty()) { c_.clear(); return *this; }
    if (c_.size() + o.c_.size() > 200000) throw std::length_error("polynomial product too large");
    std::vector<double> r(c_.size() + o.c_.size() - 1, 0.0);
    for (std::size_t i = 0; i < c_.size(); ++i)
        for (std::size_t j = 0; j < o.c_.size(); ++j) r[i + j] += c_[i] * o.c_[j];
    c_ = std::move(r);
    trim_inplace();
    return *this;
}
Polynomial& Polynomial::operator*=(double s) {
    for (double& v : c_) v *= s;
    trim_inplace();
    return *this;
}
Polynomial& Polynomial::operator/=(double s) {
    if (s == 0.0) throw std::invalid_argument("division of polynomial by zero");
    for (double& v : c_) v /= s;
    trim_inplace();
    return *this;
}

std::pair<Polynomial, Polynomial> Polynomial::divmod(const Polynomial& a, const Polynomial& b) {
    if (b.is_zero()) throw std::invalid_argument("division by zero polynomial");
    if (a.degree() < b.degree()) return {Polynomial(), a};
    std::vector<double> rem = a.c_;
    std::vector<double> quot(static_cast<std::size_t>(a.degree() - b.degree()) + 1, 0.0);
    const double lead = b.leading();
    for (std::size_t i = quot.size(); i-- > 0;) {
        const double f = rem[i + b.c_.size() - 1] / lead;
        quot[i] = f;
        for (std::size_t j = 0; j < b.c_.size(); ++j) rem[i + j] -= f * b.c_[j];
    }
    rem.resize(b.c_.size() - 1);
    return {Polynomial(std::move(quot)), Polynomial(std::move(rem))};
}

Polynomial Polynomial::gcd(Polynomial a, Polynomial b, double epsilon) {
    a.trim_inplace(epsilon);
    b.trim_inplace(epsilon);
    if (a.is_zero()) return b.normalized();
    if (b.is_zero()) return a.normalized();
    int guard = 0;
    while (!b.is_zero() && guard++ < 100000) {
        Polynomial r = divmod(a, b).second;
        // scale-aware trimming of numerically-zero remainders
        const double scale = fmax(1.0, abs(b.leading()));
        r.trim_inplace(epsilon * scale);
        a = std::move(b);
        b = std::move(r);
    }
    return a.normalized();
}

Polynomial operator+(const Polynomial& a, const Polynomial& b) { Polynomial r = a; r += b; return r; }
Polynomial operator-(const Polynomial& a, const Polynomial& b) { Polynomial r = a; r -= b; return r; }
Polynomial operator*(const Polynomial& a, const Polynomial& b) { Polynomial r = a; r *= b; return r; }
Polynomial operator*(const Polynomial& a, double s) { Polynomial r = a; r *= s; return r; }
Polynomial operator*(double s, const Polynomial& a) { Polynomial r = a; r *= s; return r; }
Polynomial operator/(const Polynomial& a, double s) { Polynomial r = a; r /= s; return r; }
Polynomial operator/(const Polynomial& a, const Polynomial& b) { return Polynomial::divmod(a, b).first; }
Polynomial operator%(const Polynomial& a, const Polynomial& b) { return Polynomial::divmod(a, b).second; }
bool operator==(const Polynomial& a, const Polynomial& b) { return a.coefficients() == b.coefficients(); }
bool operator!=(const Polynomial& a, const Polynomial& b) { return !(a == b); }

// ---------------------------------------------------------------------------
// Roots: Aberth–Ehrlich simultaneous iteration with Newton polishing.
// ---------------------------------------------------------------------------
Polynomial::RootsResult Polynomial::roots(double tol, int max_iter) const {
    RootsResult out;
    Polynomial p = trimmed();
    if (p.degree() < 1) { out.converged = true; return out; }

    // Factor out zero roots so the remaining polynomial has a non-zero constant term.
    std::size_t zero_roots = 0;
    while (!p.c_.empty() && p.c_[0] == 0.0) {
        p.c_.erase(p.c_.begin());
        ++zero_roots;
    }
    for (std::size_t i = 0; i < zero_roots; ++i) out.roots.emplace_back(0.0, 0.0);
    const int n = p.degree();
    if (n < 1) {
        out.converged = true;
    } else if (n == 1) {
        out.roots.emplace_back(-p.c_[0] / p.c_[1], 0.0);
        out.converged = true;
    } else if (n == 2) {
        const double a = p.c_[2], b = p.c_[1], c = p.c_[0];
        const double disc = b * b - 4.0 * a * c;
        if (disc >= 0.0) {
            const double sq = sqrt(disc);
            // Numerically stable quadratic formula.
            const double q = -0.5 * (b + (b >= 0 ? sq : -sq));
            double r1 = q / a;
            double r2 = (q != 0.0) ? c / q : -b / a - r1;
            out.roots.emplace_back(r1, 0.0);
            out.roots.emplace_back(r2, 0.0);
        } else {
            const double re = -b / (2.0 * a), im = sqrt(-disc) / (2.0 * a);
            out.roots.emplace_back(re, im);
            out.roots.emplace_back(re, -im);
        }
        out.converged = true;
    } else {
        Polynomial m = p.normalized();
        const Polynomial dm = m.derivative();
        // Cauchy bound on root magnitude.
        double bound = 0.0;
        for (int i = 0; i < n; ++i) bound = fmax(bound, abs(m.c_[static_cast<std::size_t>(i)]));
        bound += 1.0;
        std::vector<Complex> z(static_cast<std::size_t>(n));
        for (int k = 0; k < n; ++k) {
            const double angle = constants::tau * (k + 0.25) / n + 0.4;
            z[static_cast<std::size_t>(k)] = Complex(bound * 0.5 * cos(angle), bound * 0.5 * sin(angle));
        }
        int iter = 0;
        bool done = false;
        for (; iter < max_iter && !done; ++iter) {
            double max_step = 0.0;
            for (int k = 0; k < n; ++k) {
                const Complex zk = z[static_cast<std::size_t>(k)];
                const Complex pz = m.evaluate(zk);
                if (pz == Complex(0.0, 0.0)) continue;
                const Complex dpz = dm.evaluate(zk);
                Complex w = (dpz == Complex(0.0, 0.0)) ? Complex(tol, tol) : pz / dpz;
                Complex sum(0.0, 0.0);
                for (int j = 0; j < n; ++j) {
                    if (j == k) continue;
                    Complex d = zk - z[static_cast<std::size_t>(j)];
                    if (d == Complex(0.0, 0.0)) d = Complex(tol, 0.0);
                    sum += 1.0 / d;
                }
                const Complex denom = 1.0 - w * sum;
                const Complex step = (denom == Complex(0.0, 0.0)) ? w : w / denom;
                z[static_cast<std::size_t>(k)] = zk - step;
                const double rel = cabs(step) / fmax(1.0, cabs(zk));
                if (rel > max_step) max_step = rel;
            }
            if (max_step < tol) done = true;
        }
        out.iterations = iter;
        out.converged = done;
        // Newton polish on the original (unnormalised) polynomial and real-axis snapping.
        for (Complex& r : z) {
            for (int it = 0; it < 5; ++it) {
                const Complex pz = p.evaluate(r);
                const Complex dpz = p.derivative().evaluate(r);
                if (dpz == Complex(0.0, 0.0)) break;
                const Complex step = pz / dpz;
                r -= step;
                if (cabs(step) <= tol * fmax(1.0, cabs(r))) break;
            }
            // Snap tiny imaginary parts to zero if the real value is also a root.
            if (abs(r.imag()) <= 1e-9 * fmax(1.0, abs(r.real()))) {
                const double re = r.real();
                const double pv = abs(p.evaluate(re));
                const double scale = abs(p.evaluate(Complex(re, r.imag())));
                if (pv <= 1e-8 * fmax(1.0, abs(p.leading())) + 2.0 * scale) r = Complex(re, 0.0);
            }
            out.roots.push_back(r);
        }
    }
    std::sort(out.roots.begin(), out.roots.end(), [](const Complex& a, const Complex& b) {
        if (a.real() != b.real()) return a.real() < b.real();
        return a.imag() < b.imag();
    });
    for (const Complex& r : out.roots)
        if (r.imag() == 0.0) out.real_roots.push_back(r.real());
    return out;
}

std::vector<double> Polynomial::real_roots(double tol) const { return roots(tol).real_roots; }

// ---------------------------------------------------------------------------
// Parsing "3x^2 - 2x + 1" (also accepts "3*x**2", "x², unicode minus")
// ---------------------------------------------------------------------------
Polynomial Polynomial::parse(const std::string& text, char var) {
    if (text.size() > 10000) throw std::invalid_argument("polynomial text too long");
    std::string s;
    s.reserve(text.size());
    // Normalise unicode minus/superscripts and strip whitespace and '*'.
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == '*' && i + 1 < text.size() && text[i + 1] == '*') { s.push_back('^'); ++i; continue; } // ** is power
        if (c == ' ' || c == '\t' || c == '*') continue;
        if (c == 0xE2 && i + 2 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0x88 &&
            static_cast<unsigned char>(text[i + 2]) == 0x92) { s.push_back('-'); i += 2; continue; }
        if (c == 0xC2 && i + 1 < text.size()) {
            const unsigned char d = static_cast<unsigned char>(text[i + 1]);
            if (d == 0xB2) { s += "^2"; ++i; continue; }
            if (d == 0xB3) { s += "^3"; ++i; continue; }
            if (d == 0xB9) { s += "^1"; ++i; continue; }
        }
        s.push_back(static_cast<char>(c));
    }
    if (s.empty()) throw std::invalid_argument("empty polynomial");
    Polynomial result;
    std::size_t i = 0;
    bool any = false;
    while (i < s.size()) {
        double signv = 1.0;
        while (i < s.size() && (s[i] == '+' || s[i] == '-')) {
            if (s[i] == '-') signv = -signv;
            ++i;
        }
        if (i >= s.size()) throw std::invalid_argument("dangling sign in polynomial");
        // coefficient
        double coeff = 1.0;
        bool has_coeff = false;
        const std::size_t start = i;
        while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.')) ++i;
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E') && i > start) {
            std::size_t p = i + 1;
            if (p < s.size() && (s[p] == '+' || s[p] == '-')) ++p;
            if (p < s.size() && s[p] >= '0' && s[p] <= '9') {
                i = p;
                while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
            }
        }
        if (i > start) {
            coeff = std::strtod(s.substr(start, i - start).c_str(), nullptr);
            has_coeff = true;
        }
        if (i < s.size() && s[i] == '/' ) { // fractional coefficient like 3/4
            ++i;
            const std::size_t ds = i;
            while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.')) ++i;
            if (i == ds) throw std::invalid_argument("invalid fraction in polynomial");
            const double den = std::strtod(s.substr(ds, i - ds).c_str(), nullptr);
            if (den == 0.0) throw std::invalid_argument("division by zero in coefficient");
            coeff /= den;
        }
        int exponent = 0;
        if (i < s.size() && s[i] == var) {
            ++i;
            exponent = 1;
            if (i < s.size() && s[i] == '^') {
                ++i;
                bool neg = false;
                if (i < s.size() && s[i] == '-') { neg = true; ++i; }
                const std::size_t es = i;
                while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
                if (i == es) throw std::invalid_argument("missing exponent after '^'");
                if (i - es > 5) throw std::invalid_argument("exponent too large");
                exponent = std::atoi(s.substr(es, i - es).c_str());
                if (neg) throw std::invalid_argument("negative exponents are not polynomial");
            }
        } else if (!has_coeff) {
            throw std::invalid_argument("unexpected character '" + std::string(1, s[i]) + "' at position " +
                                        std::to_string(i));
        }
        if (exponent > 10000) throw std::length_error("polynomial degree too large");
        result.set(exponent, result.coefficient(exponent) + signv * coeff);
        any = true;
        if (i < s.size() && s[i] != '+' && s[i] != '-')
            throw std::invalid_argument("unexpected character '" + std::string(1, s[i]) + "' at position " +
                                        std::to_string(i));
    }
    if (!any) throw std::invalid_argument("empty polynomial");
    return result;
}

// ---------------------------------------------------------------------------
// Formatting
// ---------------------------------------------------------------------------
namespace {
std::string superscript(int n) {
    static const char* digits[] = {"\xE2\x81\xB0", "\xC2\xB9", "\xC2\xB2", "\xC2\xB3", "\xE2\x81\xB4",
                                   "\xE2\x81\xB5", "\xE2\x81\xB6", "\xE2\x81\xB7", "\xE2\x81\xB8", "\xE2\x81\xB9"};
    std::string s = std::to_string(n);
    std::string out;
    for (char c : s) out += digits[c - '0'];
    return out;
}
std::string format_poly(const std::vector<double>& c, char var, int precision, bool unicode) {
    if (c.empty()) return "0";
    std::string out;
    bool first = true;
    for (std::size_t i = c.size(); i-- > 0;) {
        const double v = c[i];
        if (v == 0.0) continue;
        const double a = abs(v);
        if (first) {
            if (v < 0) out += "-";
        } else {
            out += (v < 0) ? " - " : " + ";
        }
        first = false;
        const bool show_coeff = (a != 1.0) || i == 0;
        if (show_coeff) out += to_string(a, precision);
        if (i >= 1) {
            out.push_back(var);
            if (i >= 2) out += unicode ? superscript(static_cast<int>(i)) : ("^" + std::to_string(i));
        }
    }
    return out.empty() ? "0" : out;
}
} // namespace

std::string Polynomial::to_string(char var, int precision) const { return format_poly(c_, var, precision, false); }
std::string Polynomial::to_unicode(char var, int precision) const { return format_poly(c_, var, precision, true); }
std::ostream& operator<<(std::ostream& os, const Polynomial& p) { return os << p.to_string(); }

} // namespace cml
