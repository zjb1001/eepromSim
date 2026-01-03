/**
 * @file ex02_single_block_write.c
 * @brief Example 02: Single Block Write
 *
 * Learning Objectives:
 * - Understand NvM_WriteBlock asynchronous API
 * - Understand write verification mechanism
 * - Understand CRC calculation during write
 *
 * Design Reference: 05-使用示例与最佳实践.md §1.1
 *
 * Sequence:
 * 1. Initialize NvM
 * 2. Register Block with CRC protection
 * 3. Modify data in RAM
 * 4. Write Block to EEPROM (asynchronous)
 * 5. Poll for completion with verification
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

/* Block configuration */
#define BLOCK_SETTINGS_ID   1
#define BLOCK_SETTINGS_SIZE 256

/* RAM mirror buffer */
static uint8_t settings_data[BLOCK_SETTINGS_SIZE];

/* Test data pattern */
static const uint8_t test_pattern[BLOCK_SETTINGS_SIZE] = {
    [0 ... 63] = 0x01,
    [64 ... 127] = 0x02,
    [128 ... 191] = 0x03,
    [192 ... 255] = 0x04
};

/**
 * @brief Demonstrate single block write operation
 */
static void demo_single_block_write(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Example 02: Single Block Write");
    LOG_INFO("========================================");
    LOG_INFO("");

    /* Step 1: Initialize NvM */
    LOG_INFO("[Step 1] Initialize NvM...");
    NvM_Init();
    OsScheduler_Init(16);
    LOG_INFO("✓ NvM initialized");
    LOG_INFO("");

    /* Step 2: Register Block with CRC16 protection */
    LOG_INFO("[Step 2] Register Block with CRC16...");
    NvM_BlockConfig_t settings_block = {
        .block_id = BLOCK_SETTINGS_ID,
        .block_size = BLOCK_SETTINGS_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = settings_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x0400
    };

    if (NvM_RegisterBlock(&settings_block) == E_OK) {
        LOG_INFO("✓ Block %d registered", BLOCK_SETTINGS_ID);
    } else {
        LOG_ERROR("✗ Block registration failed");
        return;
    }
    LOG_INFO("");

    /* Step 3: Prepare data in RAM */
    LOG_INFO("[Step 3] Prepare data in RAM...");
    memcpy(settings_data, test_pattern, BLOCK_SETTINGS_SIZE);
    LOG_INFO("✓ Data pattern loaded to RAM:");
    LOG_INFO("  Bytes [0-63]:    0x01");
    LOG_INFO("  Bytes [64-127]:  0x02");
    LOG_INFO("  Bytes [128-191]: 0x03");
    LOG_INFO("  Bytes [192-255]: 0x04");
    LOG_INFO("");

    /* Step 4: Write Block to EEPROM */
    LOG_INFO("[Step 4] Write Block to EEPROM...");
    Std_ReturnType ret = NvM_WriteBlock(BLOCK_SETTINGS_ID, settings_data);
    if (ret == E_OK) {
        LOG_INFO("✓ WriteBlock submitted (Job queued)");
        LOG_INFO("  Process:");
        LOG_INFO("    1. Calculate CRC16");
        LOG_INFO("    2. Write data to EEPROM");
        LOG_INFO("    3. Write CRC to EEPROM");
        LOG_INFO("    4. Verify by reading back");
    } else {
        LOG_ERROR("✗ WriteBlock failed");
        return;
    }
    LOG_INFO("");

    /* Step 5: Poll for job completion */
    LOG_INFO("[Step 5] Poll for job completion...");
    uint8_t job_result;
    uint32_t iterations = 0;

    do {
        NvM_MainFunction();
        ret = NvM_GetJobResult(BLOCK_SETTINGS_ID, &job_result);
        iterations++;

        if (iterations % 10 == 0) {
            LOG_INFO("  Processing... (iteration %u)", iterations);
        }

    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    LOG_INFO("");
    LOG_INFO("✓ Job completed after %u iterations", iterations);
    LOG_INFO("");

    /* Verify result */
    LOG_INFO("[Verification] Check job result...");
    if (job_result == NVM_REQ_OK) {
        LOG_INFO("✓ Job result: NVM_REQ_OK");
        LOG_INFO("  Data successfully written and verified");
    } else if (job_result == NVM_REQ_INTEGRITY_FAILED) {
        LOG_ERROR("✗ Job result: NVM_REQ_INTEGRITY_FAILED");
        LOG_ERROR("  CRC verification failed");
    } else {
        LOG_ERROR("✗ Job result: %u", job_result);
    }
    LOG_INFO("");

    /* Step 6: Read back to verify persistence */
    LOG_INFO("[Step 6] Read back to verify persistence...");
    memset(settings_data, 0x00, BLOCK_SETTINGS_SIZE);  /* Clear RAM */

    ret = NvM_ReadBlock(BLOCK_SETTINGS_ID, settings_data);
    if (ret == E_OK) {
        iterations = 0;
        do {
            NvM_MainFunction();
            ret = NvM_GetJobResult(BLOCK_SETTINGS_ID, &job_result);
            iterations++;
        } while (job_result == NVM_REQ_PENDING && iterations < 100);

        /* Verify data */
        if (memcmp(settings_data, test_pattern, BLOCK_SETTINGS_SIZE) == 0) {
            LOG_INFO("✓ Read data matches written pattern");
            LOG_INFO("  Data integrity verified!");
        } else {
            LOG_ERROR("✗ Read data does NOT match");
            LOG_ERROR("  Expected: 0x01,0x02,0x03,0x04 pattern");
            LOG_ERROR("  Got:      0x%02X,0x%02X,0x%02X,0x%02X",
                     settings_data[0], settings_data[64],
                     settings_data[128], settings_data[192]);
        }
    }
    LOG_INFO("");

    LOG_INFO("========================================");
    LOG_INFO("  Example 02 Complete");
    LOG_INFO("========================================");
}

/**
 * @brief Main function
 */
int main(void)
{
    demo_single_block_write();
    return 0;
}
