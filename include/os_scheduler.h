/**
 * @file os_scheduler.h
 * @brief Virtual OS scheduler interface
 *
 * REQ-虚拟OS调度器: design/02-NvM架构设计.md §4
 * - 周期任务调度
 * - 优先级抢占
 * - 虚拟时钟与时间倍速
 * - 中断模拟
 */

#ifndef OS_SCHEDULER_H
#define OS_SCHEDULER_H

#include "common_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Task identifier type
 */
typedef uint8_t OsTaskId_t;

/**
 * @brief Task priority type (0=highest, 255=lowest)
 */
typedef uint8_t OsTaskPriority_t;

/**
 * @brief Time scale enumeration
 */
typedef enum {
    TIME_SCALE_1X = 1,         /**< Real-time (1ms virtual = 1ms actual) */
    TIME_SCALE_10X = 10,       /**< 10x speed (accelerated testing) */
    TIME_SCALE_100X = 100,     /**< 100x speed (fast simulation) */
    TIME_SCALE_FASTEST = 65535 /**< Fastest mode (no delays) */
} OsTimeScale_t;

/**
 * @brief Task state enumeration
 */
typedef enum {
    OS_TASK_SUSPENDED = 0,  /**< Task suspended */
    OS_TASK_READY = 1,      /**< Task ready to run */
    OS_TASK_RUNNING = 2,    /**< Task currently running */
    OS_TASK_WAITING = 3     /**< Task waiting */
} OsTaskState_t;

/**
 * @brief Task function prototype
 */
typedef void (*OsTaskFunc_t)(void);

/**
 * @brief Task control block
 *
 * REQ-任务模型: 周期任务、优先级、截止期、最大执行时间
 */
typedef struct {
    OsTaskId_t task_id;             /**< Task ID (unique) */
    const char *task_name;          /**< Task name (debug) */
    uint32_t period_ms;             /**< Task period (ms), 0=one-shot */
    OsTaskPriority_t priority;      /**< Priority (0=highest) */
    OsTaskFunc_t task_func;         /**< Task function pointer */
    uint32_t max_exec_time_us;      /**< Max execution time (monitoring) */
    uint32_t deadline_relative_ms;  /**< Relative deadline (0=same as period) */
    OsTaskState_t state;            /**< Current state */
    uint32_t next_activation_ms;    /**< Next activation time (virtual) */
    uint32_t execution_count;       /**< Execution counter */
} OsTask_t;

/**
 * @brief Scheduler statistics
 */
typedef struct {
    uint32_t total_ticks;           /**< Total scheduler ticks */
    uint32_t idle_ticks;            /**< Idle ticks */
    uint32_t context_switches;      /**< Context switch count */
    uint32_t deadline_misses;       /**< Deadline miss count */
    uint32_t max_exec_time_us;      /**< Maximum execution time */
} OsSchedulerStats_t;

/**
 * @brief Initialize the OS scheduler
 *
 * @param max_tasks Maximum number of tasks
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType OsScheduler_Init(uint8_t max_tasks);

/**
 * @brief Register a task with the scheduler
 *
 * @param task Pointer to task structure
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType OsScheduler_RegisterTask(const OsTask_t *task);

/**
 * @brief Unregister a task
 *
 * @param task_id Task ID to unregister
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType OsScheduler_UnregisterTask(OsTaskId_t task_id);

/**
 * @brief Start the scheduler (main loop)
 *
 * @return E_OK on success, E_NOT_OK on failure
 *
 * REQ-调度器启动: 进入主循环，周期调用就绪任务
 */
Std_ReturnType OsScheduler_Start(void);

/**
 * @brief Stop the scheduler
 *
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType OsScheduler_Stop(void);

/**
 * @brief Get current virtual time in milliseconds
 *
 * @return Virtual time in milliseconds
 */
uint32_t OsScheduler_GetVirtualTimeMs(void);

/**
 * @brief Set time scale for simulation
 *
 * @param scale Time scale factor
 * @return E_OK on success, E_NOT_OK on failure
 *
 * REQ-时间倍速: 支持1x/10x/100x/快速模式
 */
Std_ReturnType OsScheduler_SetTimeScale(OsTimeScale_t scale);

/**
 * @brief Get current time scale
 *
 * @return Current time scale
 */
OsTimeScale_t OsScheduler_GetTimeScale(void);

/**
 * @brief Trigger scheduler tick (called by timer)
 *
 * This function advances virtual time and triggers task activation
 */
void OsScheduler_Tick(void);

/**
 * @brief Get scheduler statistics
 *
 * @param stats Pointer to statistics structure
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType OsScheduler_GetStats(OsSchedulerStats_t *stats);

/**
 * @brief Disable all interrupts (simulation)
 *
 * REQ-临界区保护: 模拟关中断
 */
void OsScheduler_DisableInterrupts(void);

/**
 * @brief Enable all interrupts (simulation)
 *
 * REQ-临界区保护: 模拟开中断
 */
void OsScheduler_EnableInterrupts(void);

/**
 * @brief Sleep current task for specified milliseconds
 *
 * @param milliseconds Sleep duration
 */
void OsScheduler_Sleep(uint32_t milliseconds);

/**
 * @brief Destroy scheduler and cleanup
 */
void OsScheduler_Destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* OS_SCHEDULER_H */
