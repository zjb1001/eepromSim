/**
 * @file memif.c
 * @brief Memory Abstraction Layer Implementation
 *
 * REQ-MemIf抽象层: design/02-NvM架构设计.md §1.1
 * - 统一的内存访问接口
 * - 驱动抽象(当前仅支持EEPROM)
 * - 异步作业处理
 */

#include "memif.h"
#include "eeprom_driver.h"
#include "logging.h"
#include <string.h>

/**
 * @brief Current active job
 */
static MemIf_Job_t g_current_job = {0};

/**
 * @brief Job status
 */
static MemIf_JobStatus_t g_job_status = MEMIF_JOB_OK;

/**
 * @brief Job result
 */
static Std_ReturnType g_job_result = E_OK;

/**
 * @brief Initialize MemIf layer
 */
Std_ReturnType MemIf_Init(void)
{
    LOG_INFO("MemIf: Initializing...");

    /* Initialize underlying EEPROM driver */
    Std_ReturnType ret = Eep_Init(NULL);
    if (ret != E_OK) {
        LOG_ERROR("MemIf: EEPROM initialization failed");
        return E_NOT_OK;
    }

    /* Reset job state */
    memset(&g_current_job, 0, sizeof(MemIf_Job_t));
    g_job_status = MEMIF_JOB_OK;
    g_job_result = E_OK;

    LOG_INFO("MemIf: Initialization complete");
    return E_OK;
}

/**
 * @brief Read data from memory device
 */
Std_ReturnType MemIf_Read(uint32_t address, uint8_t *data_buffer, uint32_t length)
{
    if (data_buffer == NULL) {
        LOG_ERROR("MemIf: Read failed - NULL buffer");
        return E_NOT_OK;
    }

    LOG_DEBUG("MemIf: Read %u bytes from address 0x%X", length, address);

    /* Currently only EEPROM is supported */
    Std_ReturnType ret = Eep_Read(address, data_buffer, length);

    if (ret != E_OK) {
        LOG_ERROR("MemIf: Read failed at address 0x%X", address);
        return E_NOT_OK;
    }

    return E_OK;
}

/**
 * @brief Write data to memory device
 */
Std_ReturnType MemIf_Write(uint32_t address, const uint8_t *data_buffer, uint32_t length)
{
    if (data_buffer == NULL) {
        LOG_ERROR("MemIf: Write failed - NULL buffer");
        return E_NOT_OK;
    }

    LOG_DEBUG("MemIf: Write %u bytes to address 0x%X", length, address);

    /* Currently only EEPROM is supported */
    Std_ReturnType ret = Eep_Write(address, data_buffer, length);

    if (ret != E_OK) {
        LOG_ERROR("MemIf: Write failed at address 0x%X", address);
        return E_NOT_OK;
    }

    return E_OK;
}

/**
 * @brief Erase memory block
 */
Std_ReturnType MemIf_Erase(uint32_t address, uint32_t length)
{
    LOG_DEBUG("MemIf: Erase %u bytes at address 0x%X", length, address);

    /* EEPROM must be erased in block units */
    if (!Eep_IsBlockAligned(address)) {
        LOG_ERROR("MemIf: Erase failed - address not block-aligned");
        return E_NOT_OK;
    }

    /* Erase the block */
    Std_ReturnType ret = Eep_Erase(address);

    if (ret != E_OK) {
        LOG_ERROR("MemIf: Erase failed at address 0x%X", address);
        return E_NOT_OK;
    }

    return E_OK;
}

/**
 * @brief Get current job status
 */
MemIf_JobStatus_t MemIf_GetJobStatus(void)
{
    return g_job_status;
}

/**
 * @brief Get current job result
 */
Std_ReturnType MemIf_GetJobResult(void)
{
    return g_job_result;
}

/**
 * @brief Cancel current job
 */
Std_ReturnType MemIf_CancelJob(void)
{
    if (g_job_status == MEMIF_JOB_PENDING) {
        LOG_INFO("MemIf: Canceling job");
        g_job_status = MEMIF_JOB_CANCELED;
        g_job_result = E_NOT_OK;
        return E_OK;
    }

    return E_NOT_OK;
}

/**
 * @brief Main function for MemIf
 */
void MemIf_MainFunction(void)
{
    /* Process current job if any */
    if (g_current_job.job_type == MEMIF_JOB_READ) {
        /* Process read job */
        g_job_result = MemIf_Read(g_current_job.address,
                                   g_current_job.data_ptr,
                                   g_current_job.length);
        g_job_status = (g_job_result == E_OK) ? MEMIF_JOB_OK : MEMIF_JOB_FAILED;
    }
    else if (g_current_job.job_type == MEMIF_JOB_WRITE) {
        /* Process write job */
        g_job_result = MemIf_Write(g_current_job.address,
                                    g_current_job.data_ptr,
                                    g_current_job.length);
        g_job_status = (g_job_result == E_OK) ? MEMIF_JOB_OK : MEMIF_JOB_FAILED;
    }
    else if (g_current_job.job_type == MEMIF_JOB_ERASE) {
        /* Process erase job */
        g_job_result = MemIf_Erase(g_current_job.address,
                                    g_current_job.length);
        g_job_status = (g_job_result == E_OK) ? MEMIF_JOB_OK : MEMIF_JOB_FAILED;
    }
}
