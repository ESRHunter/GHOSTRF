// GHOSTRF E2E Automated Verification Test Runner
// Acceptance Criteria R4: Automated non-hardware verification suite

#include "test_harness.h"
#include <iostream>

void register_keeloq_tests();
void register_sub_format_tests();
void register_telemetry_tests();

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "       GHOSTRF FIRMWARE EXPANSION - NON-HARDWARE E2E TEST SUITE       \n";
    std::cout << "======================================================================\n";
    std::cout << " Platform: Linux x86_64 / Host C++17 (No Radio Hardware Required)\n";
    std::cout << " Standards: Flipper Zero .sub, KeeLoq AN642/AN662, Oregon v2.1, TPMS\n";
    std::cout << " Scope: Acceptance Criteria R4 (Tiers 1-4 Verification)\n";

    auto startOverall = std::chrono::steady_clock::now();

    // Register and execute all 3 test suites
    register_keeloq_tests();
    register_sub_format_tests();
    register_telemetry_tests();

    auto endOverall = std::chrono::steady_clock::now();
    double totalElapsedMs = std::chrono::duration<double, std::milli>(endOverall - startOverall).count();

    auto& ctx = ::ghostrf_test::TestContext::instance();
    ctx.printSummary();

    std::cout << " Total Test Execution Time: " << std::fixed << std::setprecision(2)
              << totalElapsedMs << " ms\n\n";

    return (ctx.getFailedCount() == 0) ? 0 : 1;
}
