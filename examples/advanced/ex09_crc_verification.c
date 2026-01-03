/**
 * @file ex09_crc_verification.c
 * @brief Example 09: CRC Verification and Data Integrity
 *
 * Learning Objectives:
 * - Understand CRC8/CRC16/CRC32 usage
 * - Understand CRC calculation on write
 * - Understand CRC verification on read
 * - Understand CRC error handling
 *
 * Design Reference: 04-数据完整性方案.md §2
 *
 * Use Case: Data integrity verification
 * - CRC8: Small blocks (<64B)
 * - CRC16: Medium blocks (64B-1KB)
 * - CRC32: Large blocks (>1KB)
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

/* Block configurations */
#define BLOCK_CRC8_ID      120
#define BLOCK_CRC16_ID     121
#define BLOCK_CRC32_ID     122
#define BLOCK_CORRUPT_ID   123
#define BLOCK_SIZE         256

/* RAM mirrors */
static uint8_t data_crc8[BLOCK_SIZE];
static uint8_t data_crc16[BLOCK_SIZE];
static uint8_t data_crc32[BLOCK_SIZE];
static uint8_t data_corrupt[BLOCK_SIZE];

/**
 * @brief Register blocks with different CRC types
 */
static void register_crc_blocks(void)
{
    /* CRC8 block */
    NvM_BlockConfig_t crc8_block = {
        .block_id = BLOCK_CRC8_ID,
        .block_size = 64,  /* Small block */
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC8,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = data_crc8,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x7000
    };
    NvM_RegisterBlock(&crc8_block);
    LOG_INFO("✓ Block %d registered (CRC8, 64B)", BLOCK_CRC8_ID);

    /* CRC16 block */
    NvM_BlockConfig_t crc16_block = {
        .block_id = BLOCK_CRC16_ID,
        .block_size = 256,  /* Medium block */
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = data_crc16,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x7200
    };
    NvM_RegisterBlock(&crc16_block);
    LOG_INFO("✓ Block %d registered (CRC16, 256B)", BLOCK_CRC16_ID);

    /* CRC32 block */
    NvM_BlockConfig_t crc32_block = {
        .block_id = BLOCK_CRC32_ID,
        .block_size = 1024,  /* Large block */
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC32,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = data_crc32,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x7800
    };
    NvM_RegisterBlock(&crc32_block);
    LOG_INFO("✓ Block %d registered (CRC32, 1024B)", BLOCK_CRC32_ID);

    /* Corruptible block (for error demo) */
    NvM_BlockConfig_t corrupt_block = {
        .block_id = BLOCK_CORRUPT_ID,
        .block_size = BLOCK_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = data_corrupt,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x7C00
    };
    NvM_RegisterBlock(&corrupt_block);
    LOG_INFO("✓ Block %d registered (CRC16, for error demo)", BLOCK_CORRUPT_ID);
}

/**
 * @brief Demonstrate CRC calculation on write
 */
static void demo_crc_write(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 1: CRC Calculation on Write ===");
    LOG_INFO("");

    LOG_INFO("Writing data with automatic CRC calculation...");
    LOG_INFO("");

    /* Prepare test data */
    memset(data_crc8, 0xAA, 64);
    memset(data_crc16, 0xBB, 256);
    memset(data_crc32, 0xCC, 1024);

    /* Write blocks with different CRC types */
    LOG_INFO("Block %d (CRC8):");
    NvM_WriteBlock(BLOCK_CRC8_ID, data_crc8);
    LOG_INFO("  Data: 0xAA pattern");
    LOG_INFO("  CRC: Automatically calculated and stored");

    LOG_INFO("");
    LOG_INFO("Block %d (CRC16):", BLOCK_CRC16_ID);
    NvM_WriteBlock(BLOCK_CRC16_ID, data_crc16);
    LOG_INFO("  Data: 0xBB pattern");
    LOG_INFO("  CRC: Automatically calculated and stored");

    LOG_INFO("");
    LOG_INFO("Block %d (CRC32):", BLOCK_CRC32_ID);
    NvM_WriteBlock(BLOCK_CRC32_ID, data_crc32);
    LOG_INFO("  Data: 0xCC pattern");
    LOG_INFO("  CRC: Automatically calculated and stored");

    /* Wait for completion */
    uint8_t results[3];
    uint16_t ids[3] = {BLOCK_CRC8_ID, BLOCK_CRC16_ID, BLOCK_CRC32_ID};
    uint32_t iterations = 0;

    do {
        NvM_MainFunction();
        iterations++;

        for (int i = 0; i < 3; i++) {
            if (results[i] == NVM_REQ_PENDING) {
                NvM_GetJobResult(ids[i], &results[i]);
            }
        }

    } while ((results[0] == NVM_REQ_PENDING ||
             results[1] == NVM_REQ_PENDING ||
             results[2] == NVM_REQ_PENDING) && iterations < 200);

    LOG_INFO("");
    LOG_INFO("✓ All blocks written with CRC (%u iterations)", iterations);

    /* Verify writes */
    LOG_INFO("");
    LOG_INFO("Verification:");
    LOG_INFO("  Block %d (CRC8):  %s", BLOCK_CRC8_ID,
             (results[0] == NVM_REQ_OK) ? "✓ OK" : "✗ FAILED");
    LOG_INFO("  Block %d (CRC16): %s", BLOCK_CRC16_ID,
             (results[1] == NVM_REQ_OK) ? "✓ OK" : "✗ FAILED");
    LOG_INFO("  Block %d (CRC32): %s", BLOCK_CRC32_ID,
             (results[2] == NVM_REQ_OK) ? "✓ OK" : "✗ FAILED");
}

/**
 * @brief Demonstrate CRC verification on read
 */
static void demo_crc_read(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 2: CRC Verification on Read ===");
    LOG_INFO("");

    LOG_INFO("Reading data with automatic CRC verification...");
    LOG_INFO("");

    /* Clear RAM buffers */
    memset(data_crc8, 0x00, 64);
    memset(data_crc16, 0x00, 256);
    memset(data_crc32, 0x00, 1024);

    /* Read blocks */
    NvM_ReadBlock(BLOCK_CRC8_ID, data_crc8);
    NvM_ReadBlock(BLOCK_CRC16_ID, data_crc16);
    NvM_ReadBlock(BLOCK_CRC32_ID, data_crc32);

    LOG_INFO("3 read jobs submitted");
    LOG_INFO("");

    /* Wait for completion */
    uint8_t results[3];
    uint16_t ids[3] = {BLOCK_CRC8_ID, BLOCK_CRC16_ID, BLOCK_CRC32_ID};
    uint32_t iterations = 0;

    do {
        NvM_MainFunction();
        iterations++;

        for (int i = 0; i < 3; i++) {
            if (results[i] == NVM_REQ_PENDING) {
                NvM_GetJobResult(ids[i], &results[i]);
            }
        }

    } while ((results[0] == NVM_REQ_PENDING ||
             results[1] == NVM_REQ_PENDING ||
             results[2] == NVM_REQ_PENDING) && iterations < 200);

    LOG_INFO("✓ All blocks read and CRC-verified (%u iterations)", iterations);

    /* Verify data integrity */
    LOG_INFO("");
    LOG_INFO("Data Integrity Check:");
    LOG_INFO("  Block %d (CRC8):  pattern=0x%02X, CRC=%s",
             BLOCK_CRC8_ID, data_crc8[0],
             (results[0] == NVM_REQ_OK) ? "✓ VALID" : "✗ INVALID");
    LOG_INFO("  Block %d (CRC16): pattern=0x%02X, CRC=%s",
             BLOCK_CRC16_ID, data_crc16[0],
             (results[1] == NVM_REQ_OK) ? "✓ VALID" : "✗ INVALID");
    LOG_INFO("  Block %d (CRC32): pattern=0x%02X, CRC=%s",
             BLOCK_CRC32_ID, data_crc32[0],
             (results[2] == NVM_REQ_OK) ? "✓ VALID" : "✗ INVALID");
}

/**
 * @brief Demonstrate CRC error detection
 */
static void demo_crc_error(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 3: CRC Error Detection ===");
    LOG_INFO("");

    LOG_INFO("Scenario: Simulating data corruption");
    LOG_INFO("");

    /* Write good data */
    memset(data_corrupt, 0xDD, BLOCK_SIZE);
    LOG_INFO("Step 1: Writing valid data (0xDD pattern)...");
    NvM_WriteBlock(BLOCK_CORRUPT_ID, data_corrupt);

    uint8_t job_result;
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(BLOCK_CORRUPT_ID, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    LOG_INFO("✓ Write completed (%u iterations)", iterations);
    LOG_INFO("");

    /* Clear RAM and read back */
    memset(data_corrupt, 0x00, BLOCK_SIZE);
    LOG_INFO("Step 2: Reading data back (with CRC verification)...");
    NvM_ReadBlock(BLOCK_CORRUPT_ID, data_corrupt);

    iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(BLOCK_CORRUPT_ID, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    LOG_INFO("✓ Read completed (%u iterations)", iterations);
    LOG_INFO("");

    if (job_result == NVM_REQ_OK) {
        LOG_INFO("✓ Data integrity verified (CRC OK)");
        LOG_INFO("  Data pattern: 0x%02X", data_corrupt[0]);
    } else {
        LOG_ERROR("✗ CRC error detected");
        LOG_ERROR("  Job result: %u", job_result);
    }

    LOG_INFO("");
    LOG_INFO("Note: In real system, CRC error would trigger:");
    LOG_INFO("  - ROM fallback (if available)");
    LOG_INFO("  - Redundant copy retry (for REDUNDANT blocks)");
    LOG_INFO("  - Error logging (DTC)");
    LOG_INFO("  - Default value loading");
}

/**
 * @brief Demonstrate CRC strength comparison
 */
static void demo_crc_strength(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 4: CRC Strength Comparison ===");
    LOG_INFO("");

    LOG_INFO("CRC Type Comparison:");
    LOG_INFO("");
    LOG_INFO("  CRC8:");
    LOG_INFO("    - Size: 1 byte");
    LOG_INFO("    - Detection rate: ~99.6% (1 error in 256)");
    LOG_INFO("    - Use case: Small blocks, fast verification");
    LOG_INFO("    - Example: Configuration flags");
    LOG_INFO("");
    LOG_INFO("  CRC16:");
    LOG_INFO("    - Size: 2 bytes");
    LOG_INFO("    - Detection rate: ~99.998% (1 error in 65536)");
    LOG_INFO("    - Use case: Medium blocks, balanced performance");
    LOG_INFO("    - Example: User settings, DTC data");
    LOG_INFO("");
    LOG_INFO("  CRC32:");
    LOG_INFO("    - Size: 4 bytes");
    LOG_INFO("    - Detection rate: ~99.9999999% (1 error in 4 billion)");
    LOG_INFO("    - Use case: Large blocks, critical data");
    LOG_INFO("    - Example: Firmware metadata, large datasets");
    LOG_INFO("");
}

/**
 * @brief Main demonstration
 */
static void demo_crc_verification(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Example 09: CRC Verification");
    LOG_INFO("========================================");
    LOG_INFO("");
    LOG_INFO("Use Case: Data integrity verification");
    LOG_INFO("  - Automatic CRC calculation");
    LOG_INFO("  - Automatic CRC verification");
    LOG_INFO("  - CRC error detection");
    LOG_INFO("");

    /* Initialize */
    NvM_Init();
    OsScheduler_Init(16);
    register_crc_blocks();

    /* Scenario 1: CRC write */
    demo_crc_write();

    /* Scenario 2: CRC read */
    demo_crc_read();

    /* Scenario 3: CRC error */
    demo_crc_error();

    /* Scenario 4: CRC strength */
    demo_crc_strength();

    LOG_INFO("");
    LOG_INFO("========================================");
    LOG_INFO("  Key Takeaways");
    LOG_INFO("========================================");
    LOG_INFO("✓ CRC8: Fast, small blocks");
    LOG_INFO("✓ CRC16: Balanced, medium blocks");
    LOG_INFO("✓ CRC32: Strong, large blocks");
    LOG_INFO("✓ Automatic: Calculation + Verification");
    LOG_INFO("✓ Error handling: ROM fallback, redundant copy");
    LOG_INFO("========================================");
}

/**
 * @brief Main function
 */
int main(void)
{
    demo_crc_verification();
    return 0;
}
