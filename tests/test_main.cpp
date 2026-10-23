// tests/test_main.cpp — runs every registered TEST_CASE.
#include "cml_test.hpp"

#include <cstring>
#include <exception>

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0;
    for (const auto& c : cmltest::registry()) {
        if (filter && std::strstr(c.name, filter) == nullptr) continue;
        cmltest::state().current = c.name;
        const int before = cmltest::state().failures;
        try {
            c.fn();
        } catch (const std::exception& e) {
            cmltest::report_failure("<test>", 0, std::string("unhandled exception: ") + e.what());
        } catch (...) {
            cmltest::report_failure("<test>", 0, "unhandled non-standard exception");
        }
        ++run;
        std::printf("%s %s\n", cmltest::state().failures == before ? "ok  " : "FAIL", c.name);
    }
    std::printf("\n%d test cases, %d checks, %d failures\n", run, cmltest::state().checks,
                cmltest::state().failures);
    return cmltest::state().failures == 0 ? 0 : 1;
}
