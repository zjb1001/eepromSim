/**
 * @file nvm.h
 * @brief NvM (Non-Volatile Memory) Manager Interface
 *
 * REQ-NvM核心模块: design/02-NvM架构设计.md, design/03-Block管理机制.md
 * - AUTOSAR 4.3兼容接口
 * - Block管理(Native/Redundant/Dataset)
 * - Job队列与优先级调度
 * - 状态机管理
 */

#ifndef NVM_H
#define NVM_H

#include "common_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Block types
 */
typedef enum {
    NVM_BLOCK_NATIVE = 0,      /**< Single copy block */
    NVM_BLOCK_REDUNDANT = 1,   /**< Redundant block (dual copy) */
    NVM_BLOCK_DATASET = 2      /**< Dataset block (multi-version) */
} NvM_BlockType_t;

/**
 * @brief CRC types
 */
typedef enum {
    NVM_CRC_NONE = 0,
    NVM_CRC8 = 1,
    NVM_CRC16 = 2,
    NVM_CRC32 = 3
} NvM_CrcType_t;

/**
 * @brief Block states
 */
typedef enum {
    NVM_BLOCKSTATE_UNINITIALIZED = 0,
    NVM_BLOCKSTATE_VALID = 1,
    NVM_BLOCKSTATE_INVALID = 2,
    NVM_BLOCKSTATE_RECOVERING = 3,
    NVM_BLOCKSTATE_RECOVERED = 4
} NvM_BlockStateType_t;

/**
 * @brief NvM job types
 */
typedef enum {
    NVM_JOB_READ = 0,
    NVM_JOB_WRITE = 1,
    NVM_JOB_READ_ALL = 2,
    NVM_JOB_WRITE_ALL = 3
} NvM_JobType_t;

/**
 * @brief NvM job descriptor
 */
typedef struct {
    NvM_JobType_t job_type;
    uint8_t block_id;
    uint8_t priority;           /**< 0=highest, 255=lowest */
    uint8_t is_immediate;        /**< Immediate flag */
    void *data_ptr;
    uint32_t submit_time_ms;
    uint32_t timeout_ms;
    uint8_t retry_count;
    uint8_t max_retries;
} NvM_Job_t;

/**
 * @brief Block configuration
 */
typedef struct {
    uint8_t block_id;
    uint16_t block_size;
    NvM_BlockType_t block_type;
    NvM_CrcType_t crc_type;
    uint8_t priority;
    uint8_t is_immediate;
    uint8_t is_write_protected;
    void *ram_mirror_ptr;
    const uint8_t *rom_block_ptr;
    uint32_t rom_block_size;
    uint32_t eeprom_offset;

    /* Redundant Block specific fields */
    uint32_t redundant_eeprom_offset;    /**< Backup block offset (REDUNDANT only) */
    uint32_t version_control_offset;     /**< Version control block offset (REDUNDANT only) */
    uint8_t active_version;              /**< Active version number (0-255) */

    /* Dataset Block specific fields */
    uint8_t dataset_count;               /**< Number of dataset versions (DATASET only) */
    uint8_t active_dataset_index;        /**< Currently active dataset index (DATASET only) */

    NvM_BlockStateType_t state;
    uint32_t erase_count;
} NvM_BlockConfig_t;

/**
 * @brief NvM initialization
 *
 * @return E_OK on success
 */
Std_ReturnType NvM_Init(void);

/**
 * @brief Register a block configuration
 *
 * @param block_config Block configuration
 * @return E_OK on success
 */
Std_ReturnType NvM_RegisterBlock(const NvM_BlockConfig_t *block_config);

/**
 * @brief Read a single block
 *
 * @param block_id Block ID
 * @param nvm_buffer Buffer to store data
 * @return E_OK on success (job queued), E_NOT_OK on failure
 */
Std_ReturnType NvM_ReadBlock(NvM_BlockIdType block_id, void *nvm_buffer);

/**
 * @brief Write a single block
 *
 * @param block_id Block ID
 * @param nvm_buffer Data to write
 * @return E_OK on success (job queued), E_NOT_OK on failure
 */
Std_ReturnType NvM_WriteBlock(NvM_BlockIdType block_id, const void *nvm_buffer);

/**
 * @brief Read all blocks
 *
 * @return E_OK on success (job queued), E_NOT_OK on failure
 */
Std_ReturnType NvM_ReadAll(void);

/**
 * @brief Write all blocks
 *
 * @return E_OK on success (job queued), E_NOT_OK on failure
 */
Std_ReturnType NvM_WriteAll(void);

/**
 * @brief Get error status of a block
 *
 * @param block_id Block ID
 * @param error_status_ptr Pointer to store error status
 * @return E_OK on success
 */
Std_ReturnType NvM_GetErrorStatus(NvM_BlockIdType block_id, uint8_t *error_status_ptr);

/**
 * @brief Get job result
 *
 * @param block_id Block ID
 * @param result_ptr Pointer to store result
 * @return E_OK on success
 */
Std_ReturnType NvM_GetJobResult(NvM_BlockIdType block_id, uint8_t *result_ptr);

/**
 * @brief Main function (called by scheduler)
 *
 * Processes job queue and handles state machine
 */
void NvM_MainFunction(void);

/**
 * @brief Job end notification callback
 *
 * @param block_id Block ID
 */
void NvM_JobEndNotification(NvM_BlockIdType block_id);

/**
 * @brief Job error notification callback
 *
 * @param block_id Block ID
 */
void NvM_JobErrorNotification(NvM_BlockIdType block_id);

/**
 * @brief Set data index for dataset blocks
 *
 * @param block_id Block ID
 * @param data_index Data index
 * @return E_OK on success
 */
Std_ReturnType NvM_SetDataIndex(NvM_BlockIdType block_id, uint8_t data_index);

/**
 * @brief Get diagnostics information
 *
 * @param info_ptr Pointer to diagnostics info
 * @return E_OK on success
 */
typedef struct {
    uint32_t total_jobs_processed;
    uint32_t total_jobs_failed;
    uint32_t total_jobs_retried;
    uint32_t current_queue_depth;
    uint32_t max_queue_depth;
} NvM_Diagnostics_t;

Std_ReturnType NvM_GetDiagnostics(NvM_Diagnostics_t *info_ptr);

#ifdef __cplusplus
}
#endif

#endif /* NVM_H */
