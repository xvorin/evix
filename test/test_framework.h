#pragma once

#include <cstdio>
#include <iostream>

//
// Minimal self-contained test harness shared by all evix test binaries.
// Define test cases with TEST_CASE(name), register them in main() with
// RUN_TEST(name), and finish with TEST_SUMMARY(). A case returns true on
// success; TEST_ASSERT short-circuits with a message on failure.
//

#define TEST_ASSERT(cond, msg)                                                 \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "FAIL: " << msg << " (" << #cond << ")" << std::endl; \
            return false;                                                      \
        }                                                                      \
    } while (0)

#define TEST_CASE(name) bool test_##name()

// Global counters, defined once per translation unit that includes this header.
static int g_passed = 0;
static int g_failed = 0;

#define RUN_TEST(name)                              \
    do {                                            \
        std::cout << "Running " << #name << "... "; \
        if (test_##name()) {                        \
            std::cout << "PASSED" << std::endl;     \
            g_passed++;                             \
        } else {                                    \
            std::cout << "FAILED" << std::endl;     \
            g_failed++;                             \
        }                                           \
    } while (0)

#define TEST_SUMMARY()                                             \
    do {                                                           \
        std::cout << "========== SUMMARY ==========" << std::endl; \
        std::cout << "Passed: " << g_passed << std::endl;          \
        std::cout << "Failed: " << g_failed << std::endl;          \
    } while (0)

#define TEST_RESULT() (g_failed > 0 ? 1 : 0)
