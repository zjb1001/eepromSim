/**
 * @file nvm_basics.c
 * @brief NvM ReadBlock/WriteBlock demonstration
 *
 * Phase 2 Example: NvM Core Module
 * - Demonstrate NvM_Init, RegisterBlock
 * - Demonstrate ReadBlock/WriteBlock
 * - Demonstrate ReadAll/WriteAll
 * - Demonstrate Job queue processing
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Sample Block 0: User configuration
 */
static uint8_t block0_data[256] = {0};
static const uint8_t block0_rom[256] = {
    0x01, 0x02, 0x03, 0x04  /* Default configuration */
};

/**
 * @brief Sample Block 1: Runtime counter
 */
static uint32_t block1_counter = 0;

/* Prevent unused variable warning */
static void __attribute__((unused)) dummy_use_counter(void) {
    (void)block1_counter;
}

/**
 * @brief Demonstrate basic ReadBlock/WriteBlock
 */
static void demo_basic_read_write(void)
{
    LOG_INFO("=== Demo: Basic ReadBlock/WriteBlock ===");

    /* Initialize NvM */
    NvM_Init();

    /* Register Block 0 */
    NvM_BlockConfig_t block0 = {
        .block_id = 0,
        .block_size = 256,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = block0_data,
        .rom_block_ptr = block0_rom,
        .rom_block_size = sizeof(block0_rom),
        .eeprom_offset = 0x0000  /* Must be block-aligned (1024 bytes) */
    };

    NvM_RegisterBlock(&block0);
    LOG_INFO("Registered Block 0 (size=256, CRC16)");

    /* Prepare test data */
    memset(block0_data, 0xAA, 256);
    LOG_INFO("Prepared test data (0xAA pattern)");

    /* Write block */
    LOG_INFO("Writing Block 0...");
    Std_ReturnType ret = NvM_WriteBlock(0, block0_data);
    if (ret == E_OK) {
        LOG_INFO("✓ WriteBlock job queued");
    } else {
        LOG_ERROR("✗ WriteBlock failed");
        return;
    }

    /* Process jobs */
    for (int i = 0; i < 10; i++) {
        NvM_MainFunction();
    }

    /* Verify write result */
    uint8_t result;
    NvM_GetJobResult(0, &result);
    if (result == NVM_REQ_OK) {
        LOG_INFO("✓ Block 0 written successfully");
    } else {
        LOG_WARN("Block 0 write result: %d", result);
    }

    /* Clear RAM and read back */
    memset(block0_data, 0x00, 256);
    LOG_INFO("RAM cleared, reading back...");

    ret = NvM_ReadBlock(0, block0_data);
    if (ret == E_OK) {
        LOG_INFO("✓ ReadBlock job queued");
    }

    /* Process jobs */
    for (int i = 0; i < 10; i++) {
        NvM_MainFunction();
    }

    /* Verify read data */
    NvM_GetJobResult(0, &result);
    if (result == NVM_REQ_OK) {
        /* Check first few bytes */
        if (block0_data[0] == 0xAA && block0_data[1] == 0xAA) {
            LOG_INFO("✓ Block 0 read successfully, data verified (0x%02X 0x%02X)",
                     block0_data[0], block0_data[1]);
        } else {
            LOG_WARN("Block 0 data mismatch (0x%02X 0x%02X)",
                     block0_data[0], block0_data[1]);
        }
    }

    /* Get diagnostics */
    NvM_Diagnostics_t diag;
    NvM_GetDiagnostics(&diag);
    LOG_INFO("Diagnostics: processed=%u, failed=%u",
             diag.total_jobs_processed, diag.total_jobs_failed);

    LOG_INFO("");
}

/**
 * @brief Demonstrate multiple blocks with different priorities
 */
static void demo_multi_block_priority(void)
{
    LOG_INFO("=== Demo: Multi-Block Priority Scheduling ===");

    /* Reinitialize NvM */
    NvM_Init();

    /* Register multiple blocks with different priorities */
    static uint8_t block_a[256], block_b[256], block_c[256];

    NvM_BlockConfig_t config_a = {
        .block_id = 1,
        .block_size = 256,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 50,  /* Low priority */
        .ram_mirror_ptr = block_a,
        .eeprom_offset = 0x0400  /* 1024 bytes aligned */
    };

    NvM_BlockConfig_t config_b = {
        .block_id = 2,
        .block_size = 256,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,  /* High priority */
        .ram_mirror_ptr = block_b,
        .eeprom_offset = 0x0800  /* 2048 bytes aligned */
    };

    NvM_BlockConfig_t config_c = {
        .block_id = 3,
        .block_size = 256,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 30,  /* Medium priority */
        .ram_mirror_ptr = block_c,
        .eeprom_offset = 0x0C00  /* 3072 bytes aligned */
    };

    NvM_RegisterBlock(&config_a);
    NvM_RegisterBlock(&config_b);
    NvM_RegisterBlock(&config_c);

    LOG_INFO("Registered 3 blocks: A(Pri=50), B(Pri=10), C(Pri=30)");

    /* Submit write requests in order: A, B, C */
    memset(block_a, 0xAA, 256);
    memset(block_b, 0xBB, 256);
    memset(block_c, 0xCC, 256);

    NvM_WriteBlock(1, block_a);  /* Low priority */
    LOG_INFO("Queued WriteBlock A (priority=50)");

    NvM_WriteBlock(2, block_b);  /* High priority */
    LOG_INFO("Queued WriteBlock B (priority=10)");

    NvM_WriteBlock(3, block_c);  /* Medium priority */
    LOG_INFO("Queued WriteBlock C (priority=30)");

    /* Process jobs - should execute in order: B, C, A */
    LOG_INFO("Processing jobs (should execute: B → C → A)...");
    for (int i = 0; i < 20; i++) {
        NvM_MainFunction();
    }

    /* Get diagnostics */
    NvM_Diagnostics_t diag;
    NvM_GetDiagnostics(&diag);
    LOG_INFO("Jobs processed: %u", diag.total_jobs_processed);

    LOG_INFO("");
}

/**
 * @brief Demonstrate WriteAll
 */
static void demo_write_all(void)
{
    LOG_INFO("=== Demo: WriteAll ===");

    NvM_Init();

    /* Register 3 blocks */
    static uint8_t data[3][256];

    for (int i = 0; i < 3; i++) {
        memset(data[i], 0x10 + i, 256);

        NvM_BlockConfig_t config = {
            .block_id = i,
            .block_size = 256,
            .block_type = NVM_BLOCK_NATIVE,
            .crc_type = NVM_CRC16,
            .priority = 10,
            .ram_mirror_ptr = data[i],
            .eeprom_offset = i * 0x0400  /* Block-aligned */
        };

        NvM_RegisterBlock(&config);
    }

    LOG_INFO("Registered 3 blocks for WriteAll");

    /* Trigger WriteAll */
    LOG_INFO("Triggering WriteAll...");
    Std_ReturnType ret = NvM_WriteAll();
    if (ret == E_OK) {
        LOG_INFO("✓ WriteAll job queued");
    }

    /* Process jobs */
    for (int i = 0; i < 30; i++) {
        NvM_MainFunction();
    }

    /* Get diagnostics */
    NvM_Diagnostics_t diag;
    NvM_GetDiagnostics(&diag);
    LOG_INFO("✓ WriteAll complete: %u jobs processed", diag.total_jobs_processed);

    LOG_INFO("");
}

/**
 * @brief Main function
 */
int main(void)
{
    Log_SetLevel(LOG_LEVEL_INFO);

    LOG_INFO("========================================");
    LOG_INFO("  NvM ReadBlock/WriteBlock Example");
    LOG_INFO("========================================");
    LOG_INFO("");

    demo_basic_read_write();
    demo_multi_block_priority();
    demo_write_all();

    LOG_INFO("========================================");
    LOG_INFO("  Example completed successfully!");
    LOG_INFO("========================================");

    return 0;
}
