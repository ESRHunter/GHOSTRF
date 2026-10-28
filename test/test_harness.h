#pragma once
// GHOSTRF E2E Test Suite - Lightweight C++17 Test Harness
// Standalone, zero external dependencies, thread-safe, colored output.

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <chrono>
#include <cmath>
#include <functional>
#include <cstdint>
#include <cstring>

namespace ghostrf_test {

// ANSI Terminal Colors
inline const char* COLOR_RESET   = "\033[0m";
inline const char* COLOR_RED     = "\033[1;31m";
inline const char* COLOR_GREEN   = "\033[1;32m";
inline const char* COLOR_YELLOW  = "\033[1;33m";
inline const char* COLOR_BLUE    = "\033[1;34m";
inline const char* COLOR_MAGENTA = "\033[1;35m";
inline const char* COLOR_CYAN    = "\033[1;36m";
inline const char* COLOR_WHITE   = "\033[1;37m";
inline const char* COLOR_GRAY    = "\033[0;90m";

enum class TestTier {
    TIER_1_FEATURE = 1,
    TIER_2_BOUNDARY = 2,
    TIER_3_CROSS_FEATURE = 3,
    TIER_4_REAL_WORLD = 4
};

} // namespace ghostrf_test

using ghostrf_test::TestTier;

namespace ghostrf_test {

struct TestFailure {
    std::string file;
    int line;
    std::string expression;
    std::string message;
};

struct TestCaseResult {
    std::string name;
    TestTier tier;
    bool passed;
    double durationMs;
    std::vector<TestFailure> failures;
};

struct TestSuiteResult {
    std::string name;
    std::vector<TestCaseResult> tests;
    size_t passed = 0;
    size_t failed = 0;
    double totalDurationMs = 0.0;
};

class TestContext {
public:
    static TestContext& instance() {
        static TestContext ctx;
        return ctx;
    }

    void startSuite(const std::string& suiteName) {
        currentSuite = suiteName;
        suites.push_back({suiteName, {}, 0, 0, 0.0});
        std::cout << "\n" << COLOR_CYAN << "======================================================================" << COLOR_RESET << "\n";
        std::cout << COLOR_CYAN << " [SUITE] " << COLOR_WHITE << suiteName << COLOR_RESET << "\n";
        std::cout << COLOR_CYAN << "======================================================================" << COLOR_RESET << "\n";
    }

    void runTest(const std::string& testName, TestTier tier, std::function<void()> testFunc) {
        currentTestName = testName;
        currentTestTier = tier;
        currentFailures.clear();

        auto startTime = std::chrono::steady_clock::now();
        try {
            testFunc();
        } catch (const std::exception& e) {
            recordFailure(__FILE__, __LINE__, "Uncaught exception", e.what());
        } catch (...) {
            recordFailure(__FILE__, __LINE__, "Uncaught exception", "Unknown non-std exception");
        }
        auto endTime = std::chrono::steady_clock::now();

        double durMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        bool passed = currentFailures.empty();

        TestCaseResult res;
        res.name = testName;
        res.tier = tier;
        res.passed = passed;
        res.durationMs = durMs;
        res.failures = currentFailures;

        auto& currentSuiteRes = suites.back();
        currentSuiteRes.tests.push_back(res);
        currentSuiteRes.totalDurationMs += durMs;

        const char* tierStr = (tier == TestTier::TIER_1_FEATURE) ? "T1" :
                              (tier == TestTier::TIER_2_BOUNDARY) ? "T2" :
                              (tier == TestTier::TIER_3_CROSS_FEATURE) ? "T3" : "T4";

        if (passed) {
            currentSuiteRes.passed++;
            totalPassed++;
            std::cout << "  " << COLOR_GREEN << "[PASS]" << COLOR_RESET
                      << " " << COLOR_GRAY << "[" << tierStr << "]" << COLOR_RESET
                      << " " << std::left << std::setw(52) << testName
                      << " " << COLOR_GRAY << "(" << std::fixed << std::setprecision(2) << durMs << " ms)" << COLOR_RESET << "\n";
        } else {
            currentSuiteRes.failed++;
            totalFailed++;
            std::cout << "  " << COLOR_RED << "[FAIL]" << COLOR_RESET
                      << " " << COLOR_GRAY << "[" << tierStr << "]" << COLOR_RESET
                      << " " << COLOR_WHITE << testName << COLOR_RESET << "\n";
            for (const auto& fail : currentFailures) {
                std::cout << "         " << COLOR_RED << "-> " << fail.file << ":" << fail.line
                          << ": Assertion failed: " << fail.expression << COLOR_RESET << "\n";
                if (!fail.message.empty()) {
                    std::cout << "            " << COLOR_YELLOW << fail.message << COLOR_RESET << "\n";
                }
            }
        }
        totalCount++;
    }

    void recordFailure(const char* file, int line, const std::string& expr, const std::string& msg = "") {
        currentFailures.push_back({file, line, expr, msg});
    }

    void printSummary() const {
        std::cout << "\n" << COLOR_CYAN << "======================================================================" << COLOR_RESET << "\n";
        std::cout << COLOR_WHITE << "                      E2E TEST EXECUTION SUMMARY                      " << COLOR_RESET << "\n";
        std::cout << COLOR_CYAN << "======================================================================" << COLOR_RESET << "\n";

        // Per-tier breakdown
        size_t t1Pass = 0, t1Fail = 0;
        size_t t2Pass = 0, t2Fail = 0;
        size_t t3Pass = 0, t3Fail = 0;
        size_t t4Pass = 0, t4Fail = 0;

        for (const auto& s : suites) {
            for (const auto& t : s.tests) {
                if (t.tier == TestTier::TIER_1_FEATURE) { (t.passed ? t1Pass : t1Fail)++; }
                else if (t.tier == TestTier::TIER_2_BOUNDARY) { (t.passed ? t2Pass : t2Fail)++; }
                else if (t.tier == TestTier::TIER_3_CROSS_FEATURE) { (t.passed ? t3Pass : t3Fail)++; }
                else if (t.tier == TestTier::TIER_4_REAL_WORLD) { (t.passed ? t4Pass : t4Fail)++; }
            }
        }

        std::cout << " " << COLOR_WHITE << "Tier 1: Feature Coverage" << COLOR_RESET << "           : "
                  << COLOR_GREEN << t1Pass << " passed" << COLOR_RESET << ", "
                  << (t1Fail > 0 ? COLOR_RED : COLOR_GRAY) << t1Fail << " failed" << COLOR_RESET << "\n";
        std::cout << " " << COLOR_WHITE << "Tier 2: Boundary & Corner Cases" << COLOR_RESET << "    : "
                  << COLOR_GREEN << t2Pass << " passed" << COLOR_RESET << ", "
                  << (t2Fail > 0 ? COLOR_RED : COLOR_GRAY) << t2Fail << " failed" << COLOR_RESET << "\n";
        std::cout << " " << COLOR_WHITE << "Tier 3: Cross-Feature Interactions" << COLOR_RESET << " : "
                  << COLOR_GREEN << t3Pass << " passed" << COLOR_RESET << ", "
                  << (t3Fail > 0 ? COLOR_RED : COLOR_GRAY) << t3Fail << " failed" << COLOR_RESET << "\n";
        std::cout << " " << COLOR_WHITE << "Tier 4: Real-World Scenarios" << COLOR_RESET << "       : "
                  << COLOR_GREEN << t4Pass << " passed" << COLOR_RESET << ", "
                  << (t4Fail > 0 ? COLOR_RED : COLOR_GRAY) << t4Fail << " failed" << COLOR_RESET << "\n";
        std::cout << COLOR_GRAY << "----------------------------------------------------------------------" << COLOR_RESET << "\n";

        for (const auto& s : suites) {
            std::cout << " Suite: " << std::left << std::setw(30) << s.name
                      << " | " << COLOR_GREEN << std::setw(3) << s.passed << " passed" << COLOR_RESET
                      << " | " << (s.failed > 0 ? COLOR_RED : COLOR_GRAY) << std::setw(2) << s.failed << " failed" << COLOR_RESET
                      << " | " << COLOR_GRAY << std::fixed << std::setprecision(2) << s.totalDurationMs << " ms" << COLOR_RESET << "\n";
        }

        std::cout << COLOR_CYAN << "======================================================================" << COLOR_RESET << "\n";
        if (totalFailed == 0) {
            std::cout << COLOR_GREEN << " OVERALL STATUS: ALL " << totalPassed << " TESTS PASSED (100.0% SUCCESS)" << COLOR_RESET << "\n";
        } else {
            std::cout << COLOR_RED << " OVERALL STATUS: " << totalFailed << " OF " << totalCount << " TESTS FAILED" << COLOR_RESET << "\n";
        }
        std::cout << COLOR_CYAN << "======================================================================" << COLOR_RESET << "\n\n";
    }

    size_t getFailedCount() const { return totalFailed; }
    size_t getPassedCount() const { return totalPassed; }
    size_t getTotalCount() const { return totalCount; }

private:
    TestContext() = default;
    std::string currentSuite;
    std::string currentTestName;
    TestTier currentTestTier = TestTier::TIER_1_FEATURE;
    std::vector<TestFailure> currentFailures;
    std::vector<TestSuiteResult> suites;
    size_t totalCount = 0;
    size_t totalPassed = 0;
    size_t totalFailed = 0;
};

} // namespace ghostrf_test

// Assertion Macros
#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            ::ghostrf_test::TestContext::instance().recordFailure(__FILE__, __LINE__, #cond, "Expected true, got false"); \
        } \
    } while (0)

#define ASSERT_FALSE(cond) \
    do { \
        if (cond) { \
            ::ghostrf_test::TestContext::instance().recordFailure(__FILE__, __LINE__, #cond, "Expected false, got true"); \
        } \
    } while (0)

#define ASSERT_EQ(expected, actual) \
    do { \
        auto _exp = (expected); \
        auto _act = (actual); \
        if (!(_exp == _act)) { \
            std::ostringstream _oss; \
            _oss << "Expected: " << _exp << ", Got: " << _act; \
            ::ghostrf_test::TestContext::instance().recordFailure(__FILE__, __LINE__, #expected " == " #actual, _oss.str()); \
        } \
    } while (0)

#define ASSERT_NE(val1, val2) \
    do { \
        auto _v1 = (val1); \
        auto _v2 = (val2); \
        if (_v1 == _v2) { \
            std::ostringstream _oss; \
            _oss << "Expected values to differ, both equal: " << _v1; \
            ::ghostrf_test::TestContext::instance().recordFailure(__FILE__, __LINE__, #val1 " != " #val2, _oss.str()); \
        } \
    } while (0)

#define ASSERT_NEAR(expected, actual, tol) \
    do { \
        double _exp = static_cast<double>(expected); \
        double _act = static_cast<double>(actual); \
        double _t = static_cast<double>(tol); \
        if (std::abs(_exp - _act) > _t) { \
            std::ostringstream _oss; \
            _oss << "Expected: " << _exp << " +/- " << _t << ", Got: " << _act << " (diff=" << std::abs(_exp - _act) << ")"; \
            ::ghostrf_test::TestContext::instance().recordFailure(__FILE__, __LINE__, "|" #expected " - " #actual "| <= " #tol, _oss.str()); \
        } \
    } while (0)

#define ASSERT_HEX32_EQ(expected, actual) \
    do { \
        uint32_t _exp = static_cast<uint32_t>(expected); \
        uint32_t _act = static_cast<uint32_t>(actual); \
        if (_exp != _act) { \
            std::ostringstream _oss; \
            _oss << "Expected: 0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << _exp \
                 << ", Got: 0x" << std::setw(8) << std::setfill('0') << _act; \
            ::ghostrf_test::TestContext::instance().recordFailure(__FILE__, __LINE__, #expected " == " #actual, _oss.str()); \
        } \
    } while (0)

#define ASSERT_HEX64_EQ(expected, actual) \
    do { \
        uint64_t _exp = static_cast<uint64_t>(expected); \
        uint64_t _act = static_cast<uint64_t>(actual); \
        if (_exp != _act) { \
            std::ostringstream _oss; \
            _oss << "Expected: 0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << _exp \
                 << ", Got: 0x" << std::setw(16) << std::setfill('0') << _act; \
            ::ghostrf_test::TestContext::instance().recordFailure(__FILE__, __LINE__, #expected " == " #actual, _oss.str()); \
        } \
    } while (0)

#define ASSERT_STR_EQ(expected, actual) \
    do { \
        std::string _exp = (expected); \
        std::string _act = (actual); \
        if (_exp != _act) { \
            std::ostringstream _oss; \
            _oss << "Expected: \"" << _exp << "\", Got: \"" << _act << "\""; \
            ::ghostrf_test::TestContext::instance().recordFailure(__FILE__, __LINE__, #expected " == " #actual, _oss.str()); \
        } \
    } while (0)

#define ASSERT_STR_CONTAINS(haystack, needle) \
    do { \
        std::string _hay = (haystack); \
        std::string _ndl = (needle); \
        if (_hay.find(_ndl) == std::string::npos) { \
            std::ostringstream _oss; \
            _oss << "Substring \"" << _ndl << "\" not found in string of length " << _hay.length(); \
            ::ghostrf_test::TestContext::instance().recordFailure(__FILE__, __LINE__, "contains(" #haystack ", " #needle ")", _oss.str()); \
        } \
    } while (0)

// Test Registration
#define TEST_SUITE(name) \
    ::ghostrf_test::TestContext::instance().startSuite(name)

#define RUN_TEST_CASE(name, tier, func) \
    ::ghostrf_test::TestContext::instance().runTest(name, tier, func)
