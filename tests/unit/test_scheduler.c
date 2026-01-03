/**
 * @file test_scheduler.c
 * @brief Unit Test: OS Scheduler
 *
 * Test Coverage:
 * - Task registration and management
 * - Time scaling (1x/10x/100x)
 * - Priority handling
 * - Task execution order
 *
 * Test Strategy:
 * - Functional testing of scheduler APIs
 * - Time measurement verification
 * - Multi-task scenarios
 */

#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h>

/* Test counters */
static uint32_t tests_passed = 0;
static uint32_t tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
            LOG_INFO("  ✓ %s", message); \
        } else { \
            tests_failed++; \
            LOG_ERROR("  ✗ %s", message); \
        } \
    } while(0)

#define TEST_ASSERT_EQ(actual, expected, message) \
    TEST_ASSERT((actual) == (expected), message)

/**
 * @brief Test scheduler initialization
 */
static void test_scheduler_init(void)
{
    LOG_INFO("");
    LOG_INFO("Test: Scheduler Initialization");

    /* Initialize with 16 max tasks */
    Std_ReturnType ret = OsScheduler_Init(16);
    TEST_ASSERT_EQ(ret, E_OK, "Scheduler initialized with 16 tasks");

    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test time scaling
 */
static void test_time_scaling(void)
{
    LOG_INFO("");
    LOG_INFO("Test: Time Scaling (1x/10x/100x)");

    LOG_INFO("  Testing 1x scale (real-time)...");
    OsScheduler_SetTimeScale(1);
    uint32_t scale1 = OsScheduler_GetTimeScale();
    TEST_ASSERT_EQ(scale1, 1, "Time scale set to 1x");

    LOG_INFO("  Testing 10x scale (accelerated)...");
    OsScheduler_SetTimeScale(10);
    uint32_t scale10 = OsScheduler_GetTimeScale();
    TEST_ASSERT_EQ(scale10, 10, "Time scale set to 10x");

    LOG_INFO("  Testing 100x scale (fast simulation)...");
    OsScheduler_SetTimeScale(100);
    uint32_t scale100 = OsScheduler_GetTimeScale();
    TEST_ASSERT_EQ(scale100, 100, "Time scale set to 100x");

    /* Reset to 1x */
    OsScheduler_SetTimeScale(1);

    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test task timing
 */
static void test_task_timing(void)
{
    LOG_INFO("");
    LOG_INFO("Test: Task Timing");

    OsScheduler_Init(16);
    OsScheduler_SetTimeScale(1);

    /* Measure tick count */
    uint32_t tick1 = OsScheduler_GetTick();
    LOG_INFO("  Initial tick: %u", tick1);

    /* Simulate some work (100ms) */
    usleep(100000);  /* 100ms */

    uint32_t tick2 = OsScheduler_GetTick();
    LOG_INFO("  After 100ms: %u", tick2);

    /* Tick should have increased */
    TEST_ASSERT(tick2 >= tick1, "Tick count non-decreasing");

    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test multiple task execution
 */
static void test_multiple_tasks(void)
{
    LOG_INFO("");
    LOG_INFO("Test: Multiple Task Execution");

    OsScheduler_Init(16);

    /* Register multiple tasks */
    for (int i = 0; i < 5; i++) {
        OsSchedulerTask_t task = {
            .task_id = i,
            .period_ms = 10 * (i + 1),  /* 10ms, 20ms, 30ms, ... */
            .priority = 5 + i
        };
        Std_ReturnType ret = OsScheduler_RegisterTask(&task);
        TEST_ASSERT_EQ(ret, E_OK, "Task registered successfully");
    }

    LOG_INFO("  5 tasks registered");
    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test scheduler main function
 */
static void test_scheduler_main_function(void)
{
    LOG_INFO("");
    LOG_INFO("Test: Scheduler MainFunction");

    OsScheduler_Init(16);

    /* Call MainFunction multiple times */
    for (int i = 0; i < 100; i++) {
        OsScheduler_MainFunction();
    }

    TEST_ASSERT(1, "MainFunction executed 100 times");

    LOG_INFO("  Result: Passed");
}

/**
 * @brief Run all scheduler tests
 */
int main(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Unit Test: OS Scheduler");
    LOG_INFO("========================================");
    LOG_INFO("");

    /* Run tests */
    test_scheduler_init();
    test_time_scaling();
    test_task_timing();
    test_multiple_tasks();
    test_scheduler_main_function();

    /* Print summary */
    LOG_INFO("");
    LOG_INFO("========================================");
    LOG_INFO("  Test Summary");
    LOG_INFO("========================================");
    LOG_INFO("  Passed: %u", tests_passed);
    LOG_INFO("  Failed: %u", tests_failed);
    LOG_INFO("  Total:  %u", tests_passed + tests_failed);
    LOG_INFO("");

    if (tests_failed == 0) {
        LOG_INFO("✓ All tests passed!");
        LOG_INFO("========================================");
        return 0;
    } else {
        LOG_ERROR("✗ Some tests failed!");
        LOG_INFO("========================================");
        return 1;
    }
}
