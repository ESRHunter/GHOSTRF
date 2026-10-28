// GHOSTRF Tier 5 Adversarial Stress Test Runner
// Acceptance Criteria R4 & Milestone M5 White-Box Hardening

#include "test_harness.h"
#include <iostream>

void register_adversarial_tests();

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "    GHOSTRF FIRMWARE EXPANSION - TIER 5 ADVERSARIAL STRESS SUITE     \n";
    std::cout << "======================================================================\n";
    std::cout << " Platform: Linux x86_64 / Host C++17 (ASan + UBSan)\n";
    std::cout << " Focus: White-Box Hardening, Fuzzing, Replay/Rollover, Protocol Misuse\n";
    std::cout << " Scope: Tier 5 Adversarial Robustness & Crash/UB Immunity\n";

    auto startOverall = std::chrono::steady_clock::now();

    register_adversarial_tests();

    auto endOverall = std::chrono::steady_clock::now();
    double totalElapsedMs = std::chrono::duration<double, std::milli>(endOverall - startOverall).count();

    auto& ctx = ::ghostrf_test::TestContext::instance();
    ctx.printSummary();

    std::cout << " Total Test Execution Time: " << std::fixed << std::setprecision(2)
              << totalElapsedMs << " ms\n\n";

    return (ctx.getFailedCount() == 0) ? 0 : 1;
}
