/**
 * @file memif.h
 * @brief Memory Abstraction Layer (MemIf) Interface
 *
 * REQ-MemIf抽象层: design/02-NvM架构设计.md §1.1
 * - 驱动选择与隐藏
 * - 读写操作的统一接口
 * - 错误处理与传播
 */

#ifndef MEMIF_H
#define MEMIF_H

#include "common_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Memory device types
 */
typedef enum {
    MEMIF_DEVICE_EEPROM = 0,    /**< EEPROM device */
    MEMIF_DEVICE_FLASH = 1,      /**< Flash device (future) */
    MEMIF_DEVICE_RAM = 2         /**< RAM device (future) */
} MemIf_DeviceType_t;

/**
 * @brief MemIf job status
 */
typedef enum {
    MEMIF_JOB_OK = 0,            /**< Job completed successfully */
    MEMIF_JOB_FAILED = 1,         /**< Job failed */
    MEMIF_JOB_PENDING = 2,        /**< Job in progress */
    MEMIF_JOB_CANCELED = 3,       /**< Job canceled */
    MEMIF_JOB_TIMEOUT = 4         /**< Job timeout */
} MemIf_JobStatus_t;

/**
 * @brief MemIf job type
 */
typedef enum {
    MEMIF_JOB_READ = 0,
    MEMIF_JOB_WRITE = 1,
    MEMIF_JOB_ERASE = 2
} MemIf_JobType_t;

/**
 * @brief MemIf job descriptor
 */
typedef struct {
    MemIf_JobType_t job_type;
    uint32_t address;
    uint8_t *data_ptr;
    uint32_t length;
    MemIf_JobStatus_t status;
    MemIf_DeviceType_t device;
} MemIf_Job_t;

/**
 * @brief Initialize MemIf layer
 *
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType MemIf_Init(void);

/**
 * @brief Read data from memory device
 *
 * @param address Device address
 * @param data_buffer Buffer to store read data
 * @param length Number of bytes to read
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType MemIf_Read(uint32_t address, uint8_t *data_buffer, uint32_t length);

/**
 * @brief Write data to memory device
 *
 * @param address Device address
 * @param data_buffer Data to write
 * @param length Number of bytes to write
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType MemIf_Write(uint32_t address, const uint8_t *data_buffer, uint32_t length);

/**
 * @brief Erase memory block
 *
 * @param address Block start address
 * @param length Block size
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType MemIf_Erase(uint32_t address, uint32_t length);

/**
 * @brief Get current job status
 *
 * @return Current job status
 */
MemIf_JobStatus_t MemIf_GetJobStatus(void);

/**
 * @brief Get current job result
 *
 * @return Job result code
 */
Std_ReturnType MemIf_GetJobResult(void);

/**
 * @brief Cancel current job
 *
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType MemIf_CancelJob(void);

/**
 * @brief Main function for MemIf (called by scheduler)
 *
 * This function processes pending jobs and updates job status
 */
void MemIf_MainFunction(void);

#ifdef __cplusplus
}
#endif

#endif /* MEMIF_H */
