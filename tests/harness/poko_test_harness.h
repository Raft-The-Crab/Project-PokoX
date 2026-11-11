#pragma once

// Dependency-free test harness for Phase 0. Kept deliberately small so a
// clean checkout builds and tests without fetching a third-party framework.
// A real framework is pinned in third_party/DEPENDENCIES.md later.

#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace poko::test {

struct TestCase {
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

inline bool register_test(const char* name, void (*fn)()) {
    registry().push_back(TestCase{name, fn});
    return true;
}

class CheckFailure : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::ostringstream out;
        out << "check failed: " << expression << " at " << file << ':' << line;
        throw CheckFailure(out.str());
    }
}

template <typename A, typename B>
void check_eq(const A& a, const B& b, const char* a_expr, const char* b_expr,
              const char* file, int line) {
    if (!(a == b)) {
        std::ostringstream out;
        out << "check failed: " << a_expr << " == " << b_expr << " (" << a
            << " != " << b << ") at " << file << ':' << line;
        throw CheckFailure(out.str());
    }
}

inline int run_all() {
    int passed = 0;
    int failed = 0;
    for (const auto& test_case : registry()) {
        try {
            test_case.fn();
            ++passed;
            std::cout << "PASS " << test_case.name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "FAIL " << test_case.name << ": " << error.what() << '\n';
        } catch (...) {
            ++failed;
            std::cerr << "FAIL " << test_case.name << ": unknown exception\n";
        }
    }
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

}  // namespace poko::test

#define POKO_TEST(name)                                                        \
    static void poko_test_##name();                                            \
    [[maybe_unused]] static const bool poko_registered_##name =                \
        ::poko::test::register_test(#name, &poko_test_##name);                 \
    static void poko_test_##name()

#define POKO_TEST_MAIN()                                                       \
    int main() { return ::poko::test::run_all(); }

#define POKO_CHECK(expression)                                                 \
    ::poko::test::check(static_cast<bool>(expression), #expression, __FILE__, \
                        __LINE__)

#define POKO_CHECK_EQ(a, b) \
    ::poko::test::check_eq((a), (b), #a, #b, __FILE__, __LINE__)

#define POKO_CHECK_THROWS(expression, exception_type)                          \
    do {                                                                       \
        bool _poko_threw = false;                                              \
        try {                                                                  \
            (void)(expression);                                                \
        } catch (const exception_type&) {                                      \
            _poko_threw = true;                                                \
        } catch (...) {                                                        \
        }                                                                      \
        ::poko::test::check(_poko_threw,                                       \
                            #expression " throws " #exception_type, __FILE__,  \
                            __LINE__);                                         \
    } while (false)
