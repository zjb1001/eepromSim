/**
 * @file nvm_jobqueue.c
 * @brief NvM Job Queue Implementation
 *
 * REQ-Job队列管理: design/03-Block管理机制.md §4
 * - 优先级排序(0=最高优先级)
 * - FIFO for same priority
 * - ReadAll/WriteAll特殊处理
 */

#include "nvm_jobqueue.h"
#include "logging.h"
#include <string.h>

/**
 * @brief Job queue structure
 */
typedef struct {
    NvM_Job_t jobs[NVM_JOB_QUEUE_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    uint16_t max_count;         /**< Watermark */
    uint32_t overflow_count;
} NvM_JobQueue_t;

/**
 * @brief Global job queue
 */
static NvM_JobQueue_t g_job_queue = {0};

/**
 * @brief Initialize job queue
 */
Std_ReturnType NvM_JobQueue_Init(void)
{
    memset(&g_job_queue, 0, sizeof(NvM_JobQueue_t));
    g_job_queue.head = 0;
    g_job_queue.tail = 0;
    g_job_queue.count = 0;
    g_job_queue.max_count = 0;
    g_job_queue.overflow_count = 0;

    LOG_INFO("NvM JobQueue: Initialized (size=%d)", NVM_JOB_QUEUE_SIZE);
    return E_OK;
}

/**
 * @brief Calculate effective priority
 *
 * ReadAll/WriteAll get highest priority, Immediate jobs boosted
 */
static uint8_t calculate_effective_priority(const NvM_Job_t *job)
{
    uint8_t priority = job->priority;

    /* Special priority for ReadAll/WriteAll */
    if (job->job_type == NVM_JOB_READ_ALL) {
        return 0;  /* Highest priority */
    }
    if (job->job_type == NVM_JOB_WRITE_ALL) {
        return 1;  /* Second highest */
    }

    /* Immediate jobs get priority boost */
    if (job->is_immediate && priority > 2) {
        priority -= 2;
    }

    return priority;
}

/**
 * @brief Find insert position for priority-based ordering
 */
static uint16_t find_insert_position(const NvM_Job_t *job)
{
    uint8_t new_priority = calculate_effective_priority(job);

    /* Scan queue to find correct position */
    for (uint16_t i = 0; i < g_job_queue.count; i++) {
        uint16_t idx = (g_job_queue.head + i) % NVM_JOB_QUEUE_SIZE;
        uint8_t existing_priority = calculate_effective_priority(&g_job_queue.jobs[idx]);

        if (new_priority < existing_priority) {
            /* Insert before this job */
            return idx;
        }
    }

    /* Insert at end */
    return g_job_queue.tail;
}

/**
 * @brief Enqueue a job
 */
Std_ReturnType NvM_JobQueue_Enqueue(const NvM_Job_t *job)
{
    if (job == NULL) {
        return E_NOT_OK;
    }

    if (NvM_JobQueue_IsFull()) {
        g_job_queue.overflow_count++;
        LOG_WARN("NvM JobQueue: Overflow! Total overflows: %u", g_job_queue.overflow_count);
        return E_NOT_OK;
    }

    /* Find insert position based on priority */
    uint16_t insert_pos = find_insert_position(job);

    /* Shift elements to make room */
    if (g_job_queue.count > 0) {
        uint16_t shift_count = (g_job_queue.tail - insert_pos + NVM_JOB_QUEUE_SIZE) % NVM_JOB_QUEUE_SIZE;

        for (uint16_t i = shift_count; i > 0; i--) {
            uint16_t src_idx = (insert_pos + i - 1) % NVM_JOB_QUEUE_SIZE;
            uint16_t dst_idx = (insert_pos + i) % NVM_JOB_QUEUE_SIZE;

            g_job_queue.jobs[dst_idx] = g_job_queue.jobs[src_idx];
        }
    }

    /* Insert new job */
    g_job_queue.jobs[insert_pos] = *job;
    g_job_queue.tail = (g_job_queue.tail + 1) % NVM_JOB_QUEUE_SIZE;
    g_job_queue.count++;

    /* Update watermark */
    if (g_job_queue.count > g_job_queue.max_count) {
        g_job_queue.max_count = g_job_queue.count;
    }

    LOG_DEBUG("NvM JobQueue: Enqueued job type=%d, block_id=%d, priority=%d (depth=%u)",
              job->job_type, job->block_id, job->priority, g_job_queue.count);

    return E_OK;
}

/**
 * @brief Dequeue highest priority job
 */
Std_ReturnType NvM_JobQueue_Dequeue(NvM_Job_t *job_ptr)
{
    if (job_ptr == NULL) {
        return E_NOT_OK;
    }

    if (NvM_JobQueue_IsEmpty()) {
        return E_NOT_OK;
    }

    /* Dequeue from head (highest priority) */
    *job_ptr = g_job_queue.jobs[g_job_queue.head];
    g_job_queue.head = (g_job_queue.head + 1) % NVM_JOB_QUEUE_SIZE;
    g_job_queue.count--;

    LOG_DEBUG("NvM JobQueue: Dequeued job type=%d, block_id=%d (depth=%u)",
              job_ptr->job_type, job_ptr->block_id, g_job_queue.count);

    return E_OK;
}

/**
 * @brief Check if queue is empty
 */
boolean NvM_JobQueue_IsEmpty(void)
{
    return (g_job_queue.count == 0);
}

/**
 * @brief Check if queue is full
 */
boolean NvM_JobQueue_IsFull(void)
{
    return (g_job_queue.count >= NVM_JOB_QUEUE_SIZE);
}

/**
 * @brief Get current queue depth
 */
uint16_t NvM_JobQueue_GetDepth(void)
{
    return g_job_queue.count;
}

/**
 * @brief Get maximum queue depth
 */
uint16_t NvM_JobQueue_GetMaxDepth(void)
{
    return g_job_queue.max_count;
}

/**
 * @brief Check for timeout jobs
 */
uint8_t NvM_JobQueue_CheckTimeouts(uint32_t current_time_ms)
{
    uint8_t timeout_count = 0;

    for (uint16_t i = 0; i < g_job_queue.count; i++) {
        uint16_t idx = (g_job_queue.head + i) % NVM_JOB_QUEUE_SIZE;
        NvM_Job_t *job = &g_job_queue.jobs[idx];

        if (job->timeout_ms == 0) {
            continue;  /* No timeout limit */
        }

        uint32_t elapsed = current_time_ms - job->submit_time_ms;

        if (elapsed > job->timeout_ms) {
            LOG_WARN("NvM JobQueue: Job timeout (type=%d, block_id=%d, elapsed=%ums, limit=%ums)",
                     job->job_type, job->block_id, elapsed, job->timeout_ms);

            /* Mark job as failed */
            job->retry_count++;
            if (job->retry_count > job->max_retries) {
                /* Remove from queue by shifting */
                for (uint16_t j = i; j < g_job_queue.count - 1; j++) {
                    uint16_t src = (g_job_queue.head + j + 1) % NVM_JOB_QUEUE_SIZE;
                    uint16_t dst = (g_job_queue.head + j) % NVM_JOB_QUEUE_SIZE;
                    g_job_queue.jobs[dst] = g_job_queue.jobs[src];
                }
                g_job_queue.tail = (g_job_queue.tail - 1 + NVM_JOB_QUEUE_SIZE) % NVM_JOB_QUEUE_SIZE;
                g_job_queue.count--;
                i--;  /* Recheck this position */
                timeout_count++;
            }
        }
    }

    return timeout_count;
}

/**
 * @brief Reset job queue
 */
void NvM_JobQueue_Reset(void)
{
    g_job_queue.head = 0;
    g_job_queue.tail = 0;
    g_job_queue.count = 0;
    /* Keep max_count for diagnostics */
}
