/**
 * @file test_os_scheduler.c
 * @brief Unit tests for OS scheduler
 *
 * REQ-虚拟OS调度器: design/02-NvM架构设计.md §4
 * - 测试任务注册
 * - 测试优先级调度
 * - 测试时间倍速
 * - 测试统计信息
 */

#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

/**
 * @brief Test task counter
 */
static uint32_t g_task1_counter = 0;
static uint32_t g_task2_counter = 0;

/**
 * @brief Sample task function 1
 */
static void task1_func(void)
{
    g_task1_counter++;
    LOG_DEBUG("Task 1 executed (count: %u)", g_task1_counter);
}

/**
 * @brief Sample task function 2
 */
static void task2_func(void)
{
    g_task2_counter++;
    LOG_DEBUG("Task 2 executed (count: %u)", g_task2_counter);
}

/**
 * @brief Test scheduler initialization
 */
static void test_init(void)
{
    LOG_INFO("Testing scheduler initialization...");

    Std_ReturnType ret = OsScheduler_Init(10);
    assert(ret == E_OK);

    /* Test double init (should work) */
    ret = OsScheduler_Init(10);
    assert(ret == E_OK);

    OsScheduler_Destroy();

    LOG_INFO("✓ Initialization test passed");
}

/**
 * @brief Test task registration
 */
static void test_register_task(void)
{
    LOG_INFO("Testing task registration...");

    OsScheduler_Init(10);

    OsTask_t task1 = {
        .task_id = 1,
        .task_name = "Task1",
        .period_ms = 10,
        .priority = 1,
        .task_func = task1_func,
        .max_exec_time_us = 1000,
        .deadline_relative_ms = 10
    };

    Std_ReturnType ret = OsScheduler_RegisterTask(&task1);
    assert(ret == E_OK);

    /* Test duplicate task ID (should fail) */
    ret = OsScheduler_RegisterTask(&task1);
    assert(ret == E_NOT_OK);

    /* Test NULL task (should fail) */
    ret = OsScheduler_RegisterTask(NULL);
    assert(ret == E_NOT_OK);

    OsScheduler_Destroy();

    LOG_INFO("✓ Task registration test passed");
}

/**
 * @brief Test task unregistration
 */
static void test_unregister_task(void)
{
    LOG_INFO("Testing task unregistration...");

    OsScheduler_Init(10);

    OsTask_t task1 = {
        .task_id = 1,
        .task_name = "Task1",
        .period_ms = 10,
        .priority = 1,
        .task_func = task1_func,
        .max_exec_time_us = 1000
    };

    OsScheduler_RegisterTask(&task1);

    Std_ReturnType ret = OsScheduler_UnregisterTask(1);
    assert(ret == E_OK);

    /* Test unregister non-existent task (should fail) */
    ret = OsScheduler_UnregisterTask(99);
    assert(ret == E_NOT_OK);

    OsScheduler_Destroy();

    LOG_INFO("✓ Task unregistration test passed");
}

/**
 * @brief Test scheduler start/stop
 */
static void test_start_stop(void)
{
    LOG_INFO("Testing scheduler start/stop...");

    OsScheduler_Init(10);

    OsTask_t task1 = {
        .task_id = 1,
        .task_name = "Task1",
        .period_ms = 10,
        .priority = 1,
        .task_func = task1_func,
        .max_exec_time_us = 1000
    };

    OsScheduler_RegisterTask(&task1);

    /* Start scheduler */
    Std_ReturnType ret = OsScheduler_Start();
    assert(ret == E_OK);

    /* Test double start (should fail) */
    ret = OsScheduler_Start();
    assert(ret == E_NOT_OK);

    /* Stop scheduler */
    ret = OsScheduler_Stop();
    assert(ret == E_OK);

    OsScheduler_Destroy();

    LOG_INFO("✓ Start/stop test passed");
}

/**
 * @brief Test task execution
 */
static void test_task_execution(void)
{
    LOG_INFO("Testing task execution...");

    g_task1_counter = 0;
    g_task2_counter = 0;

    OsScheduler_Init(10);

    /* Both tasks same priority to allow fair scheduling */
    OsTask_t task1 = {
        .task_id = 1,
        .task_name = "Task1",
        .period_ms = 1,
        .priority = 1,
        .task_func = task1_func,
        .max_exec_time_us = 1000
    };

    OsTask_t task2 = {
        .task_id = 2,
        .task_name = "Task2",
        .period_ms = 1,
        .priority = 1,  /* Same priority as task1 */
        .task_func = task2_func,
        .max_exec_time_us = 1000
    };

    OsScheduler_RegisterTask(&task1);
    OsScheduler_RegisterTask(&task2);
    OsScheduler_Start();

    /* Run scheduler for 20 ticks (enough for both tasks) */
    for (int i = 0; i < 20; i++) {
        OsScheduler_Tick();
    }

    OsScheduler_Stop();

    /* Verify tasks executed */
    LOG_INFO("  Task 1 executions: %u", g_task1_counter);
    LOG_INFO("  Task 2 executions: %u", g_task2_counter);

    /* At least one task should have executed */
    assert((g_task1_counter + g_task2_counter) > 0);

    OsScheduler_Destroy();

    LOG_INFO("✓ Task execution test passed");
}

/**
 * @brief Test virtual time
 */
static void test_virtual_time(void)
{
    LOG_INFO("Testing virtual time...");

    OsScheduler_Init(10);

    OsTask_t task1 = {
        .task_id = 1,
        .task_name = "Task1",
        .period_ms = 10,
        .priority = 1,
        .task_func = task1_func,
        .max_exec_time_us = 1000
    };

    OsScheduler_RegisterTask(&task1);
    OsScheduler_Start();

    uint32_t time1 = OsScheduler_GetVirtualTimeMs();
    assert(time1 == 0);

    /* Advance 10 ticks (10ms) */
    for (int i = 0; i < 10; i++) {
        OsScheduler_Tick();
    }

    uint32_t time2 = OsScheduler_GetVirtualTimeMs();
    assert(time2 == 10);

    LOG_INFO("  Virtual time: %u ms", time2);

    OsScheduler_Stop();
    OsScheduler_Destroy();

    LOG_INFO("✓ Virtual time test passed");
}

/**
 * @brief Test scheduler statistics
 */
static void test_statistics(void)
{
    LOG_INFO("Testing scheduler statistics...");

    OsScheduler_Init(10);

    OsTask_t task1 = {
        .task_id = 1,
        .task_name = "Task1",
        .period_ms = 1,
        .priority = 1,
        .task_func = task1_func,
        .max_exec_time_us = 1000
    };

    OsScheduler_RegisterTask(&task1);
    OsScheduler_Start();

    /* Run scheduler */
    for (int i = 0; i < 10; i++) {
        OsScheduler_Tick();
    }

    OsSchedulerStats_t stats;
    Std_ReturnType ret = OsScheduler_GetStats(&stats);
    assert(ret == E_OK);

    LOG_INFO("  Total ticks: %u", stats.total_ticks);
    LOG_INFO("  Context switches: %u", stats.context_switches);
    LOG_INFO("  Max exec time: %u us", stats.max_exec_time_us);

    assert(stats.total_ticks > 0);
    assert(stats.context_switches > 0);

    OsScheduler_Stop();
    OsScheduler_Destroy();

    LOG_INFO("✓ Statistics test passed");
}

/**
 * @brief Test time scale
 */
static void test_time_scale(void)
{
    LOG_INFO("Testing time scale...");

    OsScheduler_Init(10);

    /* Set time scale */
    Std_ReturnType ret = OsScheduler_SetTimeScale(TIME_SCALE_10X);
    assert(ret == E_OK);

    OsTimeScale_t scale = OsScheduler_GetTimeScale();
    assert(scale == TIME_SCALE_10X);

    LOG_INFO("  Time scale: %u", scale);

    /* Set invalid scale (should still work, 0 is just ignored) */
    OsScheduler_SetTimeScale(TIME_SCALE_100X);
    scale = OsScheduler_GetTimeScale();
    assert(scale == TIME_SCALE_100X);

    OsScheduler_Destroy();

    LOG_INFO("✓ Time scale test passed");
}

/**
 * @brief Main test runner
 */
int main(void)
{
    Log_SetLevel(LOG_LEVEL_INFO);

    LOG_INFO("=== OS Scheduler Unit Tests ===");
    LOG_INFO("");

    test_init();
    test_register_task();
    test_unregister_task();
    test_start_stop();
    test_task_execution();
    test_virtual_time();
    test_statistics();
    test_time_scale();

    LOG_INFO("");
    LOG_INFO("=== All tests passed! ===");

    return 0;
}
