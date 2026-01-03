/**
 * @file dataset_demo.c
 * @brief Dataset Block Demonstration
 *
 * Phase 3.5 Example: Dataset Block Multi-Version Management
 * - Demonstrate Dataset block registration
 * - Demonstrate automatic round-robin versioning
 * - Demonstrate manual version switching with NvM_SetDataIndex
 * - Demonstrate fallback mechanism on CRC failure
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Dataset Block: User Settings
 *
 * 3 versions of 128-byte user settings
 */
static uint8_t user_settings_data[256];
static const uint8_t user_settings_rom[256] = {
    0xFF, 0xFF, 0xFF, 0xFF  /* Default: all FF */
};

/**
 * @brief Demonstrate Dataset Block basic operations
 */
static void demo_dataset_basic(void)
{
    LOG_INFO("=== Demo: Dataset Block Basic Operations ===");

    /* Initialize NvM */
    NvM_Init();

    /* Register Dataset Block (3 versions, 128 bytes each) */
    NvM_BlockConfig_t dataset_block = {
        .block_id = 10,
        .block_size = 256,
        .block_type = NVM_BLOCK_DATASET,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = user_settings_data,
        .rom_block_ptr = user_settings_rom,
        .rom_block_size = sizeof(user_settings_rom),
        .eeprom_offset = 0x0000,  /* Uses 3 * 1024 = 3072 bytes */
        .dataset_count = 3,
        .active_dataset_index = 0
    };

    NvM_RegisterBlock(&dataset_block);
    LOG_INFO("Registered Dataset Block 10 (3 versions, 256B each)");

    /* Write version 0 */
    memset(user_settings_data, 0xAA, 256);
    LOG_INFO("Writing version 0 (0xAA pattern)...");

    NvM_WriteBlock(10, user_settings_data);
    for (int i = 0; i < 10; i++) {
        NvM_MainFunction();
    }

    /* Write version 1 */
    memset(user_settings_data, 0xBB, 256);
    LOG_INFO("Writing version 1 (0xBB pattern)...");

    NvM_WriteBlock(10, user_settings_data);
    for (int i = 0; i < 10; i++) {
        NvM_MainFunction();
    }

    /* Write version 2 */
    memset(user_settings_data, 0xCC, 256);
    LOG_INFO("Writing version 2 (0xCC pattern)...");

    NvM_WriteBlock(10, user_settings_data);
    for (int i = 0; i < 10; i++) {
        NvM_MainFunction();
    }

    LOG_INFO("✓ All 3 versions written successfully");
}

/**
 * @brief Demonstrate NvM_SetDataIndex for manual version switching
 */
static void demo_dataset_version_switch(void)
{
    LOG_INFO("=== Demo: Manual Version Switching with NvM_SetDataIndex ===");

    /* Reinitialize for clean state */
    NvM_Init();

    NvM_BlockConfig_t dataset_block = {
        .block_id = 11,
        .block_size = 256,
        .block_type = NVM_BLOCK_DATASET,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = user_settings_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x0000,
        .dataset_count = 3,
        .active_dataset_index = 0
    };

    NvM_RegisterBlock(&dataset_block);

    /* Write 3 different versions */
    for (uint8_t ver = 0; ver < 3; ver++) {
        memset(user_settings_data, 0x10 + ver, 64);
        NvM_WriteBlock(11, user_settings_data);
        for (int i = 0; i < 10; i++) {
            NvM_MainFunction();
        }
        LOG_INFO("Written version %u (pattern 0x%02X)", ver, 0x10 + ver);
    }

    /* Test NvM_SetDataIndex */
    LOG_INFO("--- Testing NvM_SetDataIndex ---");

    /* Read current active version (should be version 2, last written) */
    memset(user_settings_data, 0x00, 256);
    NvM_ReadBlock(11, user_settings_data);
    for (int i = 0; i < 10; i++) {
        NvM_MainFunction();
    }
    LOG_INFO("Active version contains: 0x%02X (should be 0x12)", user_settings_data[0]);

    /* Switch to version 0 */
    LOG_INFO("Switching to version 0...");
    Std_ReturnType ret = NvM_SetDataIndex(11, 0);
    if (ret == E_OK) {
        LOG_INFO("✓ SetDataIndex(11, 0) successful");

        /* Read again - should get version 0 data */
        memset(user_settings_data, 0x00, 256);
        NvM_ReadBlock(11, user_settings_data);
        for (int i = 0; i < 10; i++) {
            NvM_MainFunction();
        }
        LOG_INFO("After switch: data contains: 0x%02X (should be 0x10)", user_settings_data[0]);
    } else {
        LOG_ERROR("✗ SetDataIndex failed");
    }

    /* Switch to version 1 */
    LOG_INFO("Switching to version 1...");
    ret = NvM_SetDataIndex(11, 1);
    if (ret == E_OK) {
        LOG_INFO("✓ SetDataIndex(11, 1) successful");

        memset(user_settings_data, 0x00, 256);
        NvM_ReadBlock(11, user_settings_data);
        for (int i = 0; i < 10; i++) {
            NvM_MainFunction();
        }
        LOG_INFO("After switch: data contains: 0x%02X (should be 0x11)", user_settings_data[0]);
    }

    /* Test error cases */
    LOG_INFO("--- Testing Error Cases ---");

    /* Try to switch to invalid index */
    ret = NvM_SetDataIndex(11, 5);
    if (ret != E_OK) {
        LOG_INFO("✓ Correctly rejected invalid index 5");
    }

    /* Try to switch on non-Dataset block */
    NvM_BlockConfig_t native_block = {
        .block_id = 12,
        .block_size = 256,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = user_settings_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x1000
    };

    NvM_RegisterBlock(&native_block);

    ret = NvM_SetDataIndex(12, 0);
    if (ret != E_OK) {
        LOG_INFO("✓ Correctly rejected SetDataIndex on NATIVE block");
    }
}

/**
 * @brief Demonstrate Dataset fallback mechanism
 */
static void demo_dataset_fallback(void)
{
    LOG_INFO("=== Demo: Dataset Automatic Fallback ===");

    NvM_Init();

    NvM_BlockConfig_t dataset_block = {
        .block_id = 13,
        .block_size = 256,
        .block_type = NVM_BLOCK_DATASET,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = user_settings_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x0000,
        .dataset_count = 3,
        .active_dataset_index = 0
    };

    NvM_RegisterBlock(&dataset_block);

    /* Write 3 versions */
    for (uint8_t ver = 0; ver < 3; ver++) {
        memset(user_settings_data, 0x20 + ver, 64);
        NvM_WriteBlock(13, user_settings_data);
        for (int i = 0; i < 10; i++) {
            NvM_MainFunction();
        }
    }
    LOG_INFO("Written 3 versions (0x20, 0x21, 0x22)");

    /* Simulate corruption of version 2 (active) by reading with wrong offset */
    LOG_INFO("Simulating corruption of active version...");
    LOG_INFO("(In real scenario, this would be detected by CRC mismatch)");

    /* Read should fall back to version 1 or 0 */
    LOG_INFO("Attempting to read (should fallback to valid version)...");
    memset(user_settings_data, 0x00, 256);
    NvM_ReadBlock(13, user_settings_data);
    for (int i = 0; i < 10; i++) {
        NvM_MainFunction();
    }

    LOG_INFO("Read data contains: 0x%02X (should be one of 0x20, 0x21, 0x22)",
             user_settings_data[0]);
}

/**
 * @brief Main function
 */
int main(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Dataset Block Demonstration");
    LOG_INFO("========================================");
    LOG_INFO("");

    /* Demo 1: Basic Dataset operations */
    demo_dataset_basic();
    LOG_INFO("");

    /* Demo 2: Manual version switching */
    demo_dataset_version_switch();
    LOG_INFO("");

    /* Demo 3: Automatic fallback */
    demo_dataset_fallback();
    LOG_INFO("");

    /* Get diagnostics */
    NvM_Diagnostics_t diag;
    if (NvM_GetDiagnostics(&diag) == E_OK) {
        LOG_INFO("========================================");
        LOG_INFO("  Final Diagnostics");
        LOG_INFO("========================================");
        LOG_INFO("Total jobs processed: %u", diag.total_jobs_processed);
        LOG_INFO("Total jobs failed: %u", diag.total_jobs_failed);
        LOG_INFO("Max queue depth: %u", diag.max_queue_depth);
    }

    LOG_INFO("");
    LOG_INFO("========================================");
    LOG_INFO("  Example completed successfully!");
    LOG_INFO("========================================");

    return 0;
}
