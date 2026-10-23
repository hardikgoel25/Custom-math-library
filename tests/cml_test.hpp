// tests/cml_test.hpp — a tiny dependency-free unit test framework.
#ifndef CML_TEST_HPP
#define CML_TEST_HPP

#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace cmltest {

struct Case {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> r;
    return r;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

struct State {
    int checks = 0;
    int failures = 0;
    std::string current;
};
inline State& state() {
    static State s;
    return s;
}

inline void report_failure(const char* file, int line, const std::string& msg) {
    ++state().failures;
    std::printf("  FAIL %s:%d [%s] %s\n", file, line, state().current.c_str(), msg.c_str());
}

template <typename T>
std::string show(const T& v) {
    std::ostringstream os;
    os.precision(17);
    os << v;
    return os.str();
}

// Relative-or-absolute closeness; works for NaN (both NaN => equal) and infinities.
inline bool close(double a, double b, double rel, double abs_tol) {
    if (std::isnan(a) && std::isnan(b)) return true;
    if (std::isnan(a) || std::isnan(b)) return false;
    if (a == b) return true;
    if (std::isinf(a) || std::isinf(b)) return false;
    const double diff = std::fabs(a - b);
    if (diff <= abs_tol) return true;
    return diff <= rel * std::fmax(std::fabs(a), std::fabs(b));
}

// ULP distance between two finite doubles (for accuracy reporting).
inline double ulps(double a, double b) {
    if (a == b) return 0.0;
    if (std::isnan(a) || std::isnan(b) || std::isinf(a) || std::isinf(b)) return INFINITY;
    const double scale = std::fabs(std::nextafter(b, INFINITY) - b);
    return std::fabs(a - b) / scale;
}

} // namespace cmltest

#define CML_TEST_CONCAT2(a, b) a##b
#define CML_TEST_CONCAT(a, b) CML_TEST_CONCAT2(a, b)

#define TEST_CASE(name)                                                                            \
    static void CML_TEST_CONCAT(cml_test_fn_, __LINE__)();                                         \
    static cmltest::Registrar CML_TEST_CONCAT(cml_test_reg_, __LINE__)(name,                      \
                                                                        CML_TEST_CONCAT(cml_test_fn_, __LINE__)); \
    static void CML_TEST_CONCAT(cml_test_fn_, __LINE__)()

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        ++cmltest::state().checks;                                                                 \
        if (!(expr)) cmltest::report_failure(__FILE__, __LINE__, "CHECK(" #expr ") failed");       \
    } while (0)

#define CHECK_EQ(a, b)                                                                             \
    do {                                                                                           \
        ++cmltest::state().checks;                                                                 \
        if (!((a) == (b)))                                                                         \
            cmltest::report_failure(__FILE__, __LINE__,                                            \
                                    std::string("CHECK_EQ(" #a ", " #b "): ") + cmltest::show(a) + \
                                        " != " + cmltest::show(b));                                \
    } while (0)

#define CHECK_CLOSE(a, b, rel)                                                                     \
    do {                                                                                           \
        ++cmltest::state().checks;                                                                 \
        const double va_ = (a), vb_ = (b);                                                         \
        if (!cmltest::close(va_, vb_, (rel), 0.0))                                                 \
            cmltest::report_failure(__FILE__, __LINE__,                                            \
                                    std::string("CHECK_CLOSE(" #a ", " #b "): ") +                 \
                                        cmltest::show(va_) + " vs " + cmltest::show(vb_) +         \
                                        " (ulps=" + cmltest::show(cmltest::ulps(va_, vb_)) + ")"); \
    } while (0)

#define CHECK_CLOSE_ABS(a, b, abs_tol)                                                             \
    do {                                                                                           \
        ++cmltest::state().checks;                                                                 \
        const double va_ = (a), vb_ = (b);                                                         \
        if (!cmltest::close(va_, vb_, 0.0, (abs_tol)))                                             \
            cmltest::report_failure(__FILE__, __LINE__,                                            \
                                    std::string("CHECK_CLOSE_ABS(" #a ", " #b "): ") +             \
                                        cmltest::show(va_) + " vs " + cmltest::show(vb_));         \
    } while (0)

#define CHECK_THROWS(expr, ExceptionType)                                                          \
    do {                                                                                           \
        ++cmltest::state().checks;                                                                 \
        bool threw_ = false;                                                                       \
        try {                                                                                      \
            (void)(expr);                                                                          \
        } catch (const ExceptionType&) {                                                           \
            threw_ = true;                                                                         \
        } catch (...) {                                                                            \
        }                                                                                          \
        if (!threw_)                                                                               \
            cmltest::report_failure(__FILE__, __LINE__,                                            \
                                    "CHECK_THROWS(" #expr ") did not throw " #ExceptionType);     \
    } while (0)

#define CHECK_NOTHROW(expr)                                                                        \
    do {                                                                                           \
        ++cmltest::state().checks;                                                                 \
        try {                                                                                      \
            (void)(expr);                                                                          \
        } catch (const std::exception& e_) {                                                       \
            cmltest::report_failure(__FILE__, __LINE__,                                            \
                                    std::string("CHECK_NOTHROW(" #expr ") threw: ") + e_.what());  \
        } catch (...) {                                                                            \
            cmltest::report_failure(__FILE__, __LINE__, "CHECK_NOTHROW(" #expr ") threw");         \
        }                                                                                          \
    } while (0)

#endif // CML_TEST_HPP
