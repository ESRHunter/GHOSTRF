// GHOSTRF Challenger 2 - Empirical Stress Test Runner
// Objective: Empirically stress-test memory safety, CLI command processor, and CC1101 state machine

#include "test_harness.h"
#include <iostream>
#include <chrono>

void register_stress_streaming_tests();
void register_stress_cli_tests();
void register_stress_radio_state_tests();

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "        GHOSTRF CHALLENGER 2 - EMPIRICAL STRESS TEST HARNESS         \n";
    std::cout << "======================================================================\n";
    std::cout << " Focus: Memory Safety, Serial CLI Burst Injection, Radio State Machine\n";
    std::cout << " Environment: Host Linux x86_64 / C++17 with ASan / UBSan\n";

    auto startOverall = std::chrono::steady_clock::now();

    // Register all 3 stress test suites
    register_stress_streaming_tests();
    register_stress_cli_tests();
    register_stress_radio_state_tests();

    auto endOverall = std::chrono::steady_clock::now();
    double totalElapsedMs = std::chrono::duration<double, std::milli>(endOverall - startOverall).count();

    auto& ctx = ::ghostrf_test::TestContext::instance();
    ctx.printSummary();

    std::cout << " Total Stress Test Execution Time: " << std::fixed << std::setprecision(2)
              << totalElapsedMs << " ms\n\n";

    return (ctx.getFailedCount() == 0) ? 0 : 1;
}
