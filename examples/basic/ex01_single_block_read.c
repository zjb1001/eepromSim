/**
 * @file ex01_single_block_read.c
 * @brief Example 01: Single Block Read
 *
 * Learning Objectives:
 * - Understand NvM_ReadBlock asynchronous API
 * - Understand Job result polling mechanism
 * - Understand Native Block behavior
 *
 * Design Reference: 05-使用示例与最佳实践.md §1.1
 *
 * Sequence:
 * 1. Initialize NvM
 * 2. Register Native Block with ROM default
 * 3. Read Block (asynchronous)
 * 4. Poll for job completion
 * 5. Verify data
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

/* Block configuration */
#define BLOCK_CONFIG_ID     0
#define BLOCK_CONFIG_SIZE   256

/* ROM default data (factory settings) */
static const uint8_t rom_default[BLOCK_CONFIG_SIZE] = {
    0xDE, 0xAD, 0xBE, 0xEF,  /* Magic number */
    [1 ... 50] = 0xAA,       /* Fill pattern */
    [51 ... 255] = 0xFF      /* Remainder erased */
};

/* RAM mirror buffer */
static uint8_t config_data[BLOCK_CONFIG_SIZE];

/**
 * @brief Demonstrate single block read operation
 */
static void demo_single_block_read(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Example 01: Single Block Read");
    LOG_INFO("========================================");
    LOG_INFO("");

    /* Step 1: Initialize NvM */
    LOG_INFO("[Step 1] Initialize NvM...");
    NvM_Init();
    OsScheduler_Init(16);  /* Virtual OS with 16 task slots */
    LOG_INFO("✓ NvM initialized");
    LOG_INFO("");

    /* Step 2: Register Native Block */
    LOG_INFO("[Step 2] Register Native Block...");
    NvM_BlockConfig_t config_block = {
        .block_id = BLOCK_CONFIG_ID,
        .block_size = BLOCK_CONFIG_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = config_data,
        .rom_block_ptr = rom_default,
        .rom_block_size = sizeof(rom_default),
        .eeprom_offset = 0x0000
    };

    if (NvM_RegisterBlock(&config_block) == E_OK) {
        LOG_INFO("✓ Block %d registered (size=%u, CRC16)",
                 BLOCK_CONFIG_ID, BLOCK_CONFIG_SIZE);
    } else {
        LOG_ERROR("✗ Block registration failed");
        return;
    }
    LOG_INFO("");

    /* Step 3: Read Block (asynchronous) */
    LOG_INFO("[Step 3] Read Block from EEPROM...");
    memset(config_data, 0x00, BLOCK_CONFIG_SIZE);  /* Clear buffer */

    Std_ReturnType ret = NvM_ReadBlock(BLOCK_CONFIG_ID, config_data);
    if (ret == E_OK) {
        LOG_INFO("✓ ReadBlock submitted (Job queued)");
        LOG_INFO("  Status: PENDING (asynchronous)");
    } else {
        LOG_ERROR("✗ ReadBlock failed");
        return;
    }
    LOG_INFO("");

    /* Step 4: Poll for job completion */
    LOG_INFO("[Step 4] Poll for job completion...");
    uint8_t job_result;
    uint32_t iterations = 0;

    do {
        /* Drive NvM state machine */
        NvM_MainFunction();

        /* Check job result */
        ret = NvM_GetJobResult(BLOCK_CONFIG_ID, &job_result);
        iterations++;

        /* Small delay (simulated by processing iterations) */
        if (iterations % 10 == 0) {
            LOG_INFO("  Waiting... (iteration %u)", iterations);
        }

    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    LOG_INFO("✓ Job completed after %u iterations", iterations);
    LOG_INFO("");

    /* Step 5: Verify data */
    LOG_INFO("[Step 5] Verify data...");
    LOG_INFO("  First 4 bytes: 0x%02X 0x%02X 0x%02X 0x%02X",
             config_data[0], config_data[1], config_data[2], config_data[3]);
    LOG_INFO("  Expected:      0xDE 0xAD 0xBE 0xEF");

    if (config_data[0] == 0xDE &&
        config_data[1] == 0xAD &&
        config_data[2] == 0xBE &&
        config_data[3] == 0xEF) {
        LOG_INFO("✓ Data verification PASSED");
    } else {
        LOG_ERROR("✗ Data verification FAILED");
    }
    LOG_INFO("");

    /* Display diagnostics */
    NvM_Diagnostics_t diag;
    if (NvM_GetDiagnostics(&diag) == E_OK) {
        LOG_INFO("========================================");
        LOG_INFO("  Diagnostics");
        LOG_INFO("========================================");
        LOG_INFO("  Jobs processed: %u", diag.total_jobs_processed);
        LOG_INFO("  Jobs failed: %u", diag.total_jobs_failed);
        LOG_INFO("  Max queue depth: %u", diag.max_queue_depth);
    }

    LOG_INFO("========================================");
    LOG_INFO("  Example 01 Complete");
    LOG_INFO("========================================");
}

/**
 * @brief Main function
 */
int main(void)
{
    demo_single_block_read();
    return 0;
}
