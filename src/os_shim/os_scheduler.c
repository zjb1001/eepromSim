/**
 * @file os_scheduler.c
 * @brief Virtual OS scheduler implementation
 *
 * REQ-虚拟OS调度器: design/02-NvM架构设计.md §4
 * - 周期任务调度
 * - 优先级抢占
 * - 虚拟时钟管理
 * - 统计信息收集
 */

#include "os_scheduler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief Maximum number of tasks
 */
#define MAX_TASKS 32

/**
 * @brief Scheduler tick period in milliseconds
 */
#define SCHEDULER_TICK_MS 1

/**
 * @brief Scheduler state enumeration
 */
typedef enum {
    SCHEDULER_STOPPED = 0,
    SCHEDULER_RUNNING,
    SCHEDULER_PAUSED
} SchedulerState_t;

/**
 * @brief Global scheduler state
 */
static struct {
    SchedulerState_t state;
    OsTask_t *tasks[MAX_TASKS];
    uint8_t task_count;
    uint32_t virtual_time_ms;
    OsTimeScale_t time_scale;
    uint32_t interrupt_disable_count;
    OsSchedulerStats_t stats;
} g_scheduler = {0};

/**
 * @brief Find task by ID
 *
 * @param task_id Task ID to find
 * @return Task pointer or NULL
 */
static OsTask_t* find_task(OsTaskId_t task_id)
{
    for (uint8_t i = 0; i < g_scheduler.task_count; i++) {
        if (g_scheduler.tasks[i] != NULL &&
            g_scheduler.tasks[i]->task_id == task_id) {
            return g_scheduler.tasks[i];
        }
    }
    return NULL;
}

Std_ReturnType OsScheduler_Init(uint8_t max_tasks)
{
    (void)max_tasks; /* Reserved for future use */

    /* Reset scheduler state */
    memset(&g_scheduler, 0, sizeof(g_scheduler));
    g_scheduler.state = SCHEDULER_STOPPED;
    g_scheduler.time_scale = TIME_SCALE_1X;
    g_scheduler.virtual_time_ms = 0;

    return E_OK;
}

Std_ReturnType OsScheduler_RegisterTask(const OsTask_t *task)
{
    if (task == NULL) {
        return E_NOT_OK;
    }

    if (g_scheduler.task_count >= MAX_TASKS) {
        return E_NOT_OK;
    }

    /* Check if task ID already exists */
    if (find_task(task->task_id) != NULL) {
        return E_NOT_OK;
    }

    /* Allocate task structure */
    OsTask_t *new_task = (OsTask_t *)malloc(sizeof(OsTask_t));
    if (new_task == NULL) {
        return E_NOT_OK;
    }

    /* Copy task structure */
    memcpy(new_task, task, sizeof(OsTask_t));
    new_task->state = OS_TASK_READY;
    new_task->next_activation_ms = 0;
    new_task->execution_count = 0;

    /* Add to task list */
    g_scheduler.tasks[g_scheduler.task_count++] = new_task;

    return E_OK;
}

Std_ReturnType OsScheduler_UnregisterTask(OsTaskId_t task_id)
{
    OsTask_t *task = find_task(task_id);
    if (task == NULL) {
        return E_NOT_OK;
    }

    /* Remove from task list */
    for (uint8_t i = 0; i < g_scheduler.task_count; i++) {
        if (g_scheduler.tasks[i] == task) {
            /* Shift remaining tasks */
            for (uint8_t j = i; j < g_scheduler.task_count - 1; j++) {
                g_scheduler.tasks[j] = g_scheduler.tasks[j + 1];
            }
            g_scheduler.tasks[g_scheduler.task_count - 1] = NULL;
            g_scheduler.task_count--;

            free(task);
            return E_OK;
        }
    }

    return E_NOT_OK;
}

Std_ReturnType OsScheduler_Start(void)
{
    if (g_scheduler.state == SCHEDULER_RUNNING) {
        return E_NOT_OK;
    }

    g_scheduler.state = SCHEDULER_RUNNING;
    g_scheduler.virtual_time_ms = 0;

    /* Initialize all tasks */
    for (uint8_t i = 0; i < g_scheduler.task_count; i++) {
        OsTask_t *task = g_scheduler.tasks[i];
        if (task != NULL) {
            task->state = OS_TASK_READY;
            task->next_activation_ms = 0;
        }
    }

    return E_OK;
}

Std_ReturnType OsScheduler_Stop(void)
{
    if (g_scheduler.state != SCHEDULER_RUNNING) {
        return E_NOT_OK;
    }

    g_scheduler.state = SCHEDULER_STOPPED;
    return E_OK;
}

static OsTask_t* select_next_task(void)
{
    OsTask_t *selected = NULL;
    OsTaskPriority_t highest_priority = 255;

    /* Find highest priority ready task */
    for (uint8_t i = 0; i < g_scheduler.task_count; i++) {
        OsTask_t *task = g_scheduler.tasks[i];
        if (task != NULL && task->state == OS_TASK_READY) {
            /* Check if task should activate now */
            if (g_scheduler.virtual_time_ms >= task->next_activation_ms) {
                if (task->priority < highest_priority) {
                    highest_priority = task->priority;
                    selected = task;
                }
            }
        }
    }

    return selected;
}

void OsScheduler_Tick(void)
{
    if (g_scheduler.state != SCHEDULER_RUNNING) {
        return;
    }

    /* Advance virtual time */
    g_scheduler.virtual_time_ms += SCHEDULER_TICK_MS;
    g_scheduler.stats.total_ticks++;

    /* Check for task activations */
    for (uint8_t i = 0; i < g_scheduler.task_count; i++) {
        OsTask_t *task = g_scheduler.tasks[i];
        if (task != NULL && task->state != OS_TASK_SUSPENDED) {
            if (g_scheduler.virtual_time_ms >= task->next_activation_ms) {
                task->state = OS_TASK_READY;
            }
        }
    }

    /* Select and run highest priority ready task */
    OsTask_t *task = select_next_task();
    if (task != NULL) {
        /* Execute task */
        task->state = OS_TASK_RUNNING;

        uint32_t start_time = g_scheduler.virtual_time_ms;

        if (task->task_func != NULL) {
            task->task_func();
        }

        uint32_t exec_time_ms = g_scheduler.virtual_time_ms - start_time;
        uint32_t exec_time_us = exec_time_ms * 1000;

        /* Update statistics */
        task->execution_count++;
        g_scheduler.stats.context_switches++;

        if (exec_time_us > g_scheduler.stats.max_exec_time_us) {
            g_scheduler.stats.max_exec_time_us = exec_time_us;
        }

        /* Check for deadline miss */
        if (task->deadline_relative_ms > 0) {
            if (exec_time_ms > task->deadline_relative_ms) {
                g_scheduler.stats.deadline_misses++;
            }
        }

        /* Schedule next activation for periodic tasks */
        if (task->period_ms > 0) {
            task->next_activation_ms = g_scheduler.virtual_time_ms + task->period_ms;
            task->state = OS_TASK_READY;
        } else {
            /* One-shot task */
            task->state = OS_TASK_SUSPENDED;
        }
    } else {
        /* No ready task, idle */
        g_scheduler.stats.idle_ticks++;
    }
}

uint32_t OsScheduler_GetVirtualTimeMs(void)
{
    return g_scheduler.virtual_time_ms;
}

Std_ReturnType OsScheduler_SetTimeScale(OsTimeScale_t scale)
{
    g_scheduler.time_scale = scale;
    return E_OK;
}

OsTimeScale_t OsScheduler_GetTimeScale(void)
{
    return g_scheduler.time_scale;
}

Std_ReturnType OsScheduler_GetStats(OsSchedulerStats_t *stats)
{
    if (stats == NULL) {
        return E_NOT_OK;
    }

    *stats = g_scheduler.stats;
    return E_OK;
}

void OsScheduler_DisableInterrupts(void)
{
    g_scheduler.interrupt_disable_count++;
}

void OsScheduler_EnableInterrupts(void)
{
    if (g_scheduler.interrupt_disable_count > 0) {
        g_scheduler.interrupt_disable_count--;
    }
}

void OsScheduler_Sleep(uint32_t milliseconds)
{
    /* In simulation, just advance virtual time */
    g_scheduler.virtual_time_ms += milliseconds;
}

void OsScheduler_Destroy(void)
{
    /* Free all tasks */
    for (uint8_t i = 0; i < g_scheduler.task_count; i++) {
        if (g_scheduler.tasks[i] != NULL) {
            free(g_scheduler.tasks[i]);
            g_scheduler.tasks[i] = NULL;
        }
    }

    g_scheduler.task_count = 0;
    g_scheduler.state = SCHEDULER_STOPPED;
}
