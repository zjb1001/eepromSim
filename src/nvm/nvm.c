/**
 * @file nvm.c
 * @brief NvM Manager Implementation
 *
 * REQ-NvM核心模块: design/02-NvM架构设计.md §3
 * - 状态机实现
 * - Job处理
 * - Block管理
 */

#include "nvm.h"
#include "nvm_jobqueue.h"
#include "nvm_block_types.h"
#include "memif.h"
#include "crc16.h"
#include "eeprom_layout.h"
#include "os_scheduler.h"
#include "logging.h"
#include <string.h>

/**
 * @brief Maximum number of blocks
 */
#define NVM_MAX_BLOCKS 16

/**
 * @brief NvM instance structure
 */
typedef struct {
    NvM_BlockConfig_t blocks[NVM_MAX_BLOCKS];
    uint8_t block_count;
    NvM_Diagnostics_t diagnostics;
    boolean initialized;
} NvM_Instance_t;

/**
 * @brief Global NvM instance
 */
static NvM_Instance_t g_nvm = {0};

/**
 * @brief Job result storage
 */
static uint8_t g_job_results[NVM_MAX_BLOCKS] = {0};

/**
 * @brief Find block configuration by ID
 */
static NvM_BlockConfig_t* find_block(uint8_t block_id)
{
    for (uint8_t i = 0; i < g_nvm.block_count; i++) {
        if (g_nvm.blocks[i].block_id == block_id) {
            return &g_nvm.blocks[i];
        }
    }
    return NULL;
}

/**
 * @brief Process ReadBlock job
 */
static Std_ReturnType process_read_block(const NvM_Job_t *job)
{
    NvM_BlockConfig_t *block = find_block(job->block_id);
    if (block == NULL) {
        LOG_ERROR("NvM: Block %d not found", job->block_id);
        return E_NOT_OK;
    }

    LOG_DEBUG("NvM: Reading block %d (size=%u, type=%d)",
              block->block_id, block->block_size, block->block_type);

    /* Use block-type-specific read handlers */
    switch (block->block_type) {
        case NVM_BLOCK_NATIVE:
            return NvM_ReadNativeBlock(block, job->data_ptr);

        case NVM_BLOCK_REDUNDANT:
            return NvM_ReadRedundantBlock(block, job->data_ptr);

        case NVM_BLOCK_DATASET:
            return NvM_ReadDatasetBlock(block, job->data_ptr);

        default:
            LOG_ERROR("NvM: Unknown block type %d for block %d",
                     block->block_type, block->block_id);
            return E_NOT_OK;
    }
}

/**
 * @brief Process WriteBlock job
 */
static Std_ReturnType process_write_block(const NvM_Job_t *job)
{
    NvM_BlockConfig_t *block = find_block(job->block_id);
    if (block == NULL) {
        LOG_ERROR("NvM: Block %d not found", job->block_id);
        return E_NOT_OK;
    }

    if (block->is_write_protected) {
        LOG_WARN("NvM: Block %d is write-protected", block->block_id);
        return E_NOT_OK;
    }

    LOG_DEBUG("NvM: Writing block %d (size=%u, type=%d)",
              block->block_id, block->block_size, block->block_type);

    /* Use block-type-specific write handlers */
    switch (block->block_type) {
        case NVM_BLOCK_NATIVE:
            return NvM_WriteNativeBlock(block, job->data_ptr);

        case NVM_BLOCK_REDUNDANT:
            return NvM_WriteRedundantBlock(block, job->data_ptr);

        case NVM_BLOCK_DATASET:
            return NvM_WriteDatasetBlock(block, job->data_ptr);

        default:
            LOG_ERROR("NvM: Unknown block type %d for block %d",
                     block->block_type, block->block_id);
            return E_NOT_OK;
    }
}

/**
 * @brief Process ReadAll job
 */
static Std_ReturnType process_read_all(void)
{
    LOG_INFO("NvM: ReadAll - reading all blocks");

    Std_ReturnType ret = E_OK;
    for (uint8_t i = 0; i < g_nvm.block_count; i++) {
        NvM_BlockConfig_t *block = &g_nvm.blocks[i];

        NvM_Job_t job = {
            .job_type = NVM_JOB_READ,
            .block_id = block->block_id,
            .data_ptr = block->ram_mirror_ptr,
            .priority = 0
        };

        if (process_read_block(&job) != E_OK) {
            LOG_WARN("NvM: ReadAll - block %d failed", block->block_id);
            ret = E_NOT_OK;
        }
    }

    return ret;
}

/**
 * @brief Process WriteAll job
 */
static Std_ReturnType process_write_all(void)
{
    LOG_INFO("NvM: WriteAll - writing all blocks");

    Std_ReturnType ret = E_OK;
    for (uint8_t i = 0; i < g_nvm.block_count; i++) {
        NvM_BlockConfig_t *block = &g_nvm.blocks[i];

        if (block->is_write_protected) {
            continue;
        }

        NvM_Job_t job = {
            .job_type = NVM_JOB_WRITE,
            .block_id = block->block_id,
            .data_ptr = block->ram_mirror_ptr,
            .priority = 0
        };

        if (process_write_block(&job) != E_OK) {
            LOG_WARN("NvM: WriteAll - block %d failed", block->block_id);
            ret = E_NOT_OK;
        }
    }

    return ret;
}

/**
 * @brief Initialize NvM
 */
Std_ReturnType NvM_Init(void)
{
    LOG_INFO("NvM: Initializing...");

    /* Initialize job queue */
    NvM_JobQueue_Init();

    /* Initialize MemIf */
    MemIf_Init();

    /* Reset diagnostics */
    memset(&g_nvm.diagnostics, 0, sizeof(NvM_Diagnostics_t));

    /* Initialize default block configuration */
    g_nvm.block_count = 0;
    g_nvm.initialized = TRUE;

    LOG_INFO("NvM: Initialization complete");
    return E_OK;
}

/**
 * @brief Register a block
 */
Std_ReturnType NvM_RegisterBlock(const NvM_BlockConfig_t *block_config)
{
    if (block_config == NULL || g_nvm.block_count >= NVM_MAX_BLOCKS) {
        return E_NOT_OK;
    }

    /* Validate block layout */
    if (!EEPROM_ValidateBlockConfig((const void*)block_config)) {
        LOG_ERROR("NvM: Block %d configuration validation failed",
                 block_config->block_id);
        return E_NOT_OK;
    }

    /* Calculate and log block layout */
    Eeprom_BlockLayout_t layout;
    if (EEPROM_CalcBlockLayout((const void*)block_config, &layout) == 0) {
        LOG_DEBUG("NvM: Block %d layout: data@0x%X(%uB), crc@0x%X(%uB), slot@0x%X(%uB)",
                 block_config->block_id,
                 layout.data_offset, layout.data_size,
                 layout.crc_offset, layout.crc_size,
                 block_config->eeprom_offset, layout.slot_size);
    }

    g_nvm.blocks[g_nvm.block_count] = *block_config;
    g_nvm.blocks[g_nvm.block_count].state = NVM_BLOCKSTATE_UNINITIALIZED;
    g_nvm.blocks[g_nvm.block_count].erase_count = 0;
    g_nvm.block_count++;

    LOG_INFO("NvM: Registered block %d (type=%d, size=%u)",
             block_config->block_id, block_config->block_type, block_config->block_size);

    return E_OK;
}

/**
 * @brief Read a single block
 */
Std_ReturnType NvM_ReadBlock(NvM_BlockIdType block_id, void *nvm_buffer)
{
    if (!g_nvm.initialized) {
        return E_NOT_OK;
    }

    NvM_BlockConfig_t *block = find_block(block_id);
    if (block == NULL) {
        return E_NOT_OK;
    }

    /* Create read job */
    NvM_Job_t job = {
        .job_type = NVM_JOB_READ,
        .block_id = block_id,
        .data_ptr = nvm_buffer,
        .priority = block->priority,
        .is_immediate = block->is_immediate,
        .submit_time_ms = OsScheduler_GetVirtualTimeMs(),
        .timeout_ms = 2000,
        .retry_count = 0,
        .max_retries = 3
    };

    /* Enqueue job */
    Std_ReturnType ret = NvM_JobQueue_Enqueue(&job);
    if (ret == E_OK) {
        g_job_results[block_id] = NVM_REQ_PENDING;
    }

    return ret;
}

/**
 * @brief Write a single block
 */
Std_ReturnType NvM_WriteBlock(NvM_BlockIdType block_id, const void *nvm_buffer)
{
    if (!g_nvm.initialized) {
        return E_NOT_OK;
    }

    NvM_BlockConfig_t *block = find_block(block_id);
    if (block == NULL) {
        return E_NOT_OK;
    }

    /* Create write job */
    NvM_Job_t job = {
        .job_type = NVM_JOB_WRITE,
        .block_id = block_id,
        .data_ptr = (void*)nvm_buffer,
        .priority = block->priority,
        .is_immediate = block->is_immediate,
        .submit_time_ms = OsScheduler_GetVirtualTimeMs(),
        .timeout_ms = 3000,
        .retry_count = 0,
        .max_retries = 3
    };

    /* Enqueue job */
    Std_ReturnType ret = NvM_JobQueue_Enqueue(&job);
    if (ret == E_OK) {
        g_job_results[block_id] = NVM_REQ_PENDING;
    }

    return ret;
}

/**
 * @brief Read all blocks
 */
Std_ReturnType NvM_ReadAll(void)
{
    if (!g_nvm.initialized) {
        return E_NOT_OK;
    }

    /* Create ReadAll job */
    NvM_Job_t job = {
        .job_type = NVM_JOB_READ_ALL,
        .block_id = 0xFF,
        .data_ptr = NULL,
        .priority = 0,
        .is_immediate = TRUE,
        .submit_time_ms = OsScheduler_GetVirtualTimeMs(),
        .timeout_ms = 5000,
        .retry_count = 0,
        .max_retries = 3
    };

    return NvM_JobQueue_Enqueue(&job);
}

/**
 * @brief Write all blocks
 */
Std_ReturnType NvM_WriteAll(void)
{
    if (!g_nvm.initialized) {
        return E_NOT_OK;
    }

    /* Create WriteAll job */
    NvM_Job_t job = {
        .job_type = NVM_JOB_WRITE_ALL,
        .block_id = 0xFF,
        .data_ptr = NULL,
        .priority = 0,
        .is_immediate = TRUE,
        .submit_time_ms = OsScheduler_GetVirtualTimeMs(),
        .timeout_ms = 10000,
        .retry_count = 0,
        .max_retries = 3
    };

    return NvM_JobQueue_Enqueue(&job);
}

/**
 * @brief Get error status
 */
Std_ReturnType NvM_GetErrorStatus(NvM_BlockIdType block_id, uint8_t *error_status_ptr)
{
    if (error_status_ptr == NULL) {
        return E_NOT_OK;
    }

    NvM_BlockConfig_t *block = find_block(block_id);
    if (block == NULL) {
        *error_status_ptr = NVM_BLOCK_INVALID;
        return E_NOT_OK;
    }

    *error_status_ptr = block->state;
    return E_OK;
}

/**
 * @brief Get job result
 */
Std_ReturnType NvM_GetJobResult(NvM_BlockIdType block_id, uint8_t *result_ptr)
{
    if (result_ptr == NULL || block_id >= NVM_MAX_BLOCKS) {
        return E_NOT_OK;
    }

    *result_ptr = g_job_results[block_id];
    return E_OK;
}

/**
 * @brief Main function
 */
void NvM_MainFunction(void)
{
    if (!g_nvm.initialized) {
        return;
    }

    /* Check for timeouts */
    uint32_t current_time = OsScheduler_GetVirtualTimeMs();
    NvM_JobQueue_CheckTimeouts(current_time);

    /* Process jobs from queue */
    NvM_Job_t job;
    while (NvM_JobQueue_Dequeue(&job) == E_OK) {
        Std_ReturnType ret = E_NOT_OK;

        /* Process job based on type */
        switch (job.job_type) {
            case NVM_JOB_READ:
                ret = process_read_block(&job);
                break;

            case NVM_JOB_WRITE:
                ret = process_write_block(&job);
                break;

            case NVM_JOB_READ_ALL:
                ret = process_read_all();
                break;

            case NVM_JOB_WRITE_ALL:
                ret = process_write_all();
                break;

            default:
                LOG_ERROR("NvM: Unknown job type %d", job.job_type);
                break;
        }

        /* Update result */
        if (job.block_id != 0xFF) {
            g_job_results[job.block_id] = (ret == E_OK) ? NVM_REQ_OK : NVM_REQ_NOT_OK;
        }

        /* Update diagnostics */
        g_nvm.diagnostics.total_jobs_processed++;
        if (ret != E_OK) {
            g_nvm.diagnostics.total_jobs_failed++;
        }

        /* Notify application */
        if (ret == E_OK) {
            NvM_JobEndNotification(job.block_id);
        } else {
            NvM_JobErrorNotification(job.block_id);
        }
    }

    /* Update diagnostics */
    g_nvm.diagnostics.current_queue_depth = NvM_JobQueue_GetDepth();
}

/**
 * @brief Job end notification
 */
void NvM_JobEndNotification(NvM_BlockIdType block_id)
{
    LOG_DEBUG("NvM: Job ended for block %d", block_id);
}

/**
 * @brief Job error notification
 */
void NvM_JobErrorNotification(NvM_BlockIdType block_id)
{
    LOG_WARN("NvM: Job error for block %d", block_id);
}

/**
 * @brief Set data index (for Dataset blocks)
 *
 * Switches the active dataset index for a Dataset block.
 * This is useful for:
 * - Manual version switching
 * - Rolling back to a previous version
 * - Selecting a specific dataset after recovery
 *
 * @param block_id Block ID
 * @param data_index New dataset index (0 to dataset_count-1)
 * @return E_OK on success, E_NOT_OK on error
 */
Std_ReturnType NvM_SetDataIndex(NvM_BlockIdType block_id, uint8_t data_index)
{
    if (!g_nvm.initialized) {
        LOG_ERROR("NvM: Not initialized");
        return E_NOT_OK;
    }

    NvM_BlockConfig_t *block = find_block(block_id);
    if (block == NULL) {
        LOG_ERROR("NvM: Block %d not found", block_id);
        return E_NOT_OK;
    }

    /* Verify block is Dataset type */
    if (block->block_type != NVM_BLOCK_DATASET) {
        LOG_ERROR("NvM: Block %d is not a DATASET block (type=%d)",
                 block_id, block->block_type);
        return E_NOT_OK;
    }

    /* Validate data_index range */
    if (data_index >= block->dataset_count) {
        LOG_ERROR("NvM: Invalid data_index %u for block %d (max=%u)",
                 data_index, block_id, block->dataset_count - 1);
        return E_NOT_OK;
    }

    /* Store previous index for logging */
    uint8_t prev_index = block->active_dataset_index;

    /* Update active dataset index */
    block->active_dataset_index = data_index;

    LOG_INFO("NvM: Block %d dataset index changed: %u -> %u",
             block_id, prev_index, data_index);

    return E_OK;
}

/**
 * @brief Get diagnostics
 */
Std_ReturnType NvM_GetDiagnostics(NvM_Diagnostics_t *info_ptr)
{
    if (info_ptr == NULL) {
        return E_NOT_OK;
    }

    *info_ptr = g_nvm.diagnostics;
    info_ptr->max_queue_depth = NvM_JobQueue_GetMaxDepth();

    return E_OK;
}
