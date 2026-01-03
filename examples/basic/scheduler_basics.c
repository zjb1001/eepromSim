/**
 * @file scheduler_basics.c
 * @brief Virtual OS scheduler basic example
 *
 * REQ-虚拟OS调度器: design/02-NvM架构设计.md §4
 * - 展示任务注册
 * - 展示周期任务执行
 * - 展示优先级调度
 * - 展示统计信息
 */

#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <unistd.h>

/**
 * @brief Task 1: High priority control task (10ms period)
 */
static uint32_t task1_counter = 0;
static void control_task(void)
{
    task1_counter++;
    LOG_DEBUG("[Control Task] Execution #%u at %u ms",
              task1_counter, OsScheduler_GetVirtualTimeMs());
}

/**
 * @brief Task 2: Medium priority monitoring task (20ms period)
 */
static uint32_t task2_counter = 0;
static void monitor_task(void)
{
    task2_counter++;
    LOG_DEBUG("[Monitor Task] Execution #%u at %u ms",
              task2_counter, OsScheduler_GetVirtualTimeMs());
}

/**
 * @brief Task 3: Low priority logging task (50ms period)
 */
static uint32_t task3_counter = 0;
static void logging_task(void)
{
    task3_counter++;
    LOG_INFO("[Log Task] Execution #%u at %u ms - System running normally",
             task3_counter, OsScheduler_GetVirtualTimeMs());
}

/**
 * @brief Demonstrate basic task scheduling
 */
static void demo_basic_scheduling(void)
{
    LOG_INFO("=== Demo: Basic Task Scheduling ===");

    /* Initialize scheduler */
    OsScheduler_Init(10);

    /* Register tasks */
    OsTask_t task1 = {
        .task_id = 1,
        .task_name = "Control",
        .period_ms = 10,
        .priority = 0,      /* Highest priority */
        .task_func = control_task,
        .max_exec_time_us = 1000,
        .deadline_relative_ms = 10
    };

    OsTask_t task2 = {
        .task_id = 2,
        .task_name = "Monitor",
        .period_ms = 20,
        .priority = 1,
        .task_func = monitor_task,
        .max_exec_time_us = 500,
        .deadline_relative_ms = 20
    };

    OsTask_t task3 = {
        .task_id = 3,
        .task_name = "Logging",
        .period_ms = 50,
        .priority = 2,      /* Lowest priority */
        .task_func = logging_task,
        .max_exec_time_us = 2000,
        .deadline_relative_ms = 50
    };

    OsScheduler_RegisterTask(&task1);
    OsScheduler_RegisterTask(&task2);
    OsScheduler_RegisterTask(&task3);

    LOG_INFO("Registered 3 tasks:");
    LOG_INFO("  - Control task (priority 0, period 10ms)");
    LOG_INFO("  - Monitor task (priority 1, period 20ms)");
    LOG_INFO("  - Logging task (priority 2, period 50ms)");

    /* Start scheduler */
    OsScheduler_Start();

    /* Run for 100 ticks (100ms virtual time) */
    LOG_INFO("Running scheduler for 100ms...");
    for (int i = 0; i < 100; i++) {
        OsScheduler_Tick();
    }

    /* Stop scheduler */
    OsScheduler_Stop();

    /* Show statistics */
    LOG_INFO("");
    LOG_INFO("Execution counts:");
    LOG_INFO("  Control task: %u", task1_counter);
    LOG_INFO("  Monitor task: %u", task2_counter);
    LOG_INFO("  Logging task: %u", task3_counter);

    OsScheduler_Destroy();
}

/**
 * @brief Demonstrate scheduler statistics
 */
static void demo_scheduler_statistics(void)
{
    LOG_INFO("");
    LOG_INFO("=== Demo: Scheduler Statistics ===");

    OsScheduler_Init(10);

    /* Register a task */
    OsTask_t task = {
        .task_id = 1,
        .task_name = "TestTask",
        .period_ms = 5,
        .priority = 1,
        .task_func = control_task,
        .max_exec_time_us = 1000
    };

    OsScheduler_RegisterTask(&task);
    OsScheduler_Start();

    /* Run for 50 ticks */
    for (int i = 0; i < 50; i++) {
        OsScheduler_Tick();
    }

    /* Get statistics */
    OsSchedulerStats_t stats;
    OsScheduler_GetStats(&stats);

    LOG_INFO("Scheduler Statistics:");
    LOG_INFO("  Total ticks: %u", stats.total_ticks);
    LOG_INFO("  Idle ticks: %u", stats.idle_ticks);
    LOG_INFO("  Context switches: %u", stats.context_switches);
    LOG_INFO("  Deadline misses: %u", stats.deadline_misses);
    LOG_INFO("  Max execution time: %u us", stats.max_exec_time_us);

    OsScheduler_Destroy();
}

/**
 * @brief Demonstrate time scaling
 */
static void demo_time_scaling(void)
{
    LOG_INFO("");
    LOG_INFO("=== Demo: Time Scaling ===");

    OsScheduler_Init(10);

    /* Register task */
    OsTask_t task = {
        .task_id = 1,
        .task_name = "TimedTask",
        .period_ms = 10,
        .priority = 1,
        .task_func = control_task,
        .max_exec_time_us = 1000
    };

    OsScheduler_RegisterTask(&task);

    /* Test 1x speed */
    LOG_INFO("Testing 1x time scale (real-time)...");
    OsScheduler_SetTimeScale(TIME_SCALE_1X);
    OsScheduler_Start();

    uint32_t start = OsScheduler_GetVirtualTimeMs();
    for (int i = 0; i < 10; i++) {
        OsScheduler_Tick();
    }
    uint32_t elapsed = OsScheduler_GetVirtualTimeMs() - start;

    LOG_INFO("  Virtual time elapsed: %u ms", elapsed);

    OsScheduler_Stop();
    OsScheduler_Destroy();
}

/**
 * @brief Main function
 */
int main(void)
{
    Log_SetLevel(LOG_LEVEL_INFO);

    LOG_INFO("========================================");
    LOG_INFO("  Virtual OS Scheduler Basics Example");
    LOG_INFO("========================================");
    LOG_INFO("");

    demo_basic_scheduling();
    demo_scheduler_statistics();
    demo_time_scaling();

    LOG_INFO("");
    LOG_INFO("========================================");
    LOG_INFO("  Example completed successfully!");
    LOG_INFO("========================================");

    return 0;
}
