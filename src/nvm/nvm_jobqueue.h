/**
 * @file nvm_jobqueue.h
 * @brief NvM Job Queue Management
 *
 * REQ-Job队列管理: design/03-Block管理机制.md §4
 * - 优先级队列
 * - 超时管理
 * - 溢出处理
 */

#ifndef NVM_JOBQUEUE_H
#define NVM_JOBQUEUE_H

#include "nvm.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Job queue size
 */
#define NVM_JOB_QUEUE_SIZE 32

/**
 * @brief Initialize job queue
 *
 * @return E_OK on success
 */
Std_ReturnType NvM_JobQueue_Init(void);

/**
 * @brief Enqueue a job
 *
 * @param job Job to enqueue
 * @return E_OK on success, E_NOT_OK if queue full
 */
Std_ReturnType NvM_JobQueue_Enqueue(const NvM_Job_t *job);

/**
 * @brief Dequeue highest priority job
 *
 * @param job_ptr Pointer to store dequeued job
 * @return E_OK on success, E_NOT_OK if queue empty
 */
Std_ReturnType NvM_JobQueue_Dequeue(NvM_Job_t *job_ptr);

/**
 * @brief Check if queue is empty
 *
 * @return TRUE if empty, FALSE otherwise
 */
boolean NvM_JobQueue_IsEmpty(void);

/**
 * @brief Check if queue is full
 *
 * @return TRUE if full, FALSE otherwise
 */
boolean NvM_JobQueue_IsFull(void);

/**
 * @brief Get current queue depth
 *
 * @return Number of jobs in queue
 */
uint16_t NvM_JobQueue_GetDepth(void);

/**
 * @brief Get maximum queue depth (watermark)
 *
 * @return Maximum depth since init
 */
uint16_t NvM_JobQueue_GetMaxDepth(void);

/**
 * @brief Check for timeout jobs
 *
 * @param current_time_ms Current virtual time
 * @return Number of timeout jobs
 */
uint8_t NvM_JobQueue_CheckTimeouts(uint32_t current_time_ms);

/**
 * @brief Reset job queue
 */
void NvM_JobQueue_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* NVM_JOBQUEUE_H */
