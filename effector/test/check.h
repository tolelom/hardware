#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace check {

struct TestCase {
    const char* name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failureCount() {
    static int failures = 0;
    return failures;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body) {
        registry().push_back({name, std::move(body)});
    }
};

inline void fail(const char* file, int line, const std::string& message) {
    std::printf("  FAIL %s:%d %s\n", file, line, message.c_str());
    ++failureCount();
}

inline int runAll() {
    int failedTests = 0;
    for (const TestCase& test : registry()) {
        const int failuresBefore = failureCount();
        test.body();
        const bool passed = failureCount() == failuresBefore;
        std::printf("%s %s\n", passed ? "PASS" : "FAIL", test.name);
        if (!passed) {
            ++failedTests;
        }
    }
    std::printf("%zu tests, %d failed\n", registry().size(), failedTests);
    return failedTests == 0 ? 0 : 1;
}

}  // namespace check

#define CHECK_CONCAT_INNER(first, second) first##second
#define CHECK_CONCAT(first, second) CHECK_CONCAT_INNER(first, second)

#define TEST_CASE(name)                                                    \
    static void CHECK_CONCAT(testBody, __LINE__)();                        \
    static check::Registrar CHECK_CONCAT(testRegistrar, __LINE__)(         \
        name, CHECK_CONCAT(testBody, __LINE__));                           \
    static void CHECK_CONCAT(testBody, __LINE__)()

#define CHECK(condition)                                                   \
    do {                                                                   \
        if (!(condition)) {                                                \
            check::fail(__FILE__, __LINE__, #condition);                   \
        }                                                                  \
    } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                            \
    do {                                                                   \
        const double actualValue = (actual);                               \
        const double expectedValue = (expected);                           \
        const double toleranceValue = (tolerance);                         \
        if (!(std::fabs(actualValue - expectedValue) <= toleranceValue)) { \
            char message[200];                                             \
            std::snprintf(message, sizeof message,                         \
                          "%s = %.6f, expected %.6f +- %.6f", #actual,     \
                          actualValue, expectedValue, toleranceValue);     \
            check::fail(__FILE__, __LINE__, message);                      \
        }                                                                  \
    } while (0)
