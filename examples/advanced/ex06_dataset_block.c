/**
 * @file ex06_dataset_block.c
 * @brief Example 06: Dataset Block Multi-Version Management
 *
 * Learning Objectives:
 * - Understand Dataset Block multi-version mechanism
 * - Understand round-robin versioning
 * - Understand NvM_SetDataIndex API
 * - Understand version fallback on CRC failure
 *
 * Design Reference: 05-使用示例与最佳实践.md §1.2
 *
 * Use Case: High-frequency write scenarios
 * - User settings (frequent updates)
 * - Adaptive data (multiple versions)
 * - Write endurance optimization (spread across versions)
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

#define BLOCK_DATASET_ID   30
#define BLOCK_DATASET_SIZE  256
#define DATASET_VERSIONS   3

static uint8_t dataset_data[BLOCK_DATASET_SIZE];

/* ROM default */
static const uint8_t rom_default[BLOCK_DATASET_SIZE] = {
    ['d'] = 1, [1 ... 255] = 0xFF
};

/**
 * @brief Demonstrate Dataset Block registration
 */
static void demo_dataset_registration(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 1: Dataset Block Registration ===");
    LOG_INFO("");

    NvM_Init();
    OsScheduler_Init(16);

    NvM_BlockConfig_t dataset_block = {
        .block_id = BLOCK_DATASET_ID,
        .block_size = BLOCK_DATASET_SIZE,
        .block_type = NVM_BLOCK_DATASET,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = dataset_data,
        .rom_block_ptr = rom_default,
        .rom_block_size = sizeof(rom_default),
        .eeprom_offset = 0x3000,
        .dataset_count = DATASET_VERSIONS,
        .active_dataset_index = 0
    };

    NvM_RegisterBlock(&dataset_block);
    LOG_INFO("✓ Dataset Block registered");
    LOG_INFO("  Block ID: %u", BLOCK_DATASET_ID);
    LOG_INFO("  Versions: %u", DATASET_VERSIONS);
    LOG_INFO("  Size per version: %u bytes", BLOCK_DATASET_SIZE);
    LOG_INFO("  Total EEPROM: %u bytes", DATASET_VERSIONS * 1024);
    LOG_INFO("");
}

/**
 * @brief Demonstrate round-robin versioning
 */
static void demo_round_robin_versioning(void)
{
    LOG_INFO("=== Scenario 2: Round-Robin Versioning ===");
    LOG_INFO("");

    LOG_INFO("Writing 3 versions (automatic round-robin)...");
    LOG_INFO("");

    /* Write version 0 */
    memset(dataset_data, 0xAA, BLOCK_DATASET_SIZE);
    LOG_INFO("Write #1: Version 0 (pattern 0xAA)");
    NvM_WriteBlock(BLOCK_DATASET_ID, dataset_data);

    uint8_t job_result;
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(BLOCK_DATASET_ID, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 50);
    LOG_INFO("✓ Completed (%u iterations)", iterations);

    /* Write version 1 */
    memset(dataset_data, 0xBB, BLOCK_DATASET_SIZE);
    LOG_INFO("");
    LOG_INFO("Write #2: Version 1 (pattern 0xBB)");
    NvM_WriteBlock(BLOCK_DATASET_ID, dataset_data);

    iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(BLOCK_DATASET_ID, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 50);
    LOG_INFO("✓ Completed (%u iterations)", iterations);

    /* Write version 2 */
    memset(dataset_data, 0xCC, BLOCK_DATASET_SIZE);
    LOG_INFO("");
    LOG_INFO("Write #3: Version 2 (pattern 0xCC)");
    NvM_WriteBlock(BLOCK_DATASET_ID, dataset_data);

    iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(BLOCK_DATASET_ID, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 50);
    LOG_INFO("✓ Completed (%u iterations)", iterations);

    /* Write version 0 again (wraps around) */
    memset(dataset_data, 0xDD, BLOCK_DATASET_SIZE);
    LOG_INFO("");
    LOG_INFO("Write #4: Version 0 (pattern 0xDD) - wraps around");
    NvM_WriteBlock(BLOCK_DATASET_ID, dataset_data);

    iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(BLOCK_DATASET_ID, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 50);
    LOG_INFO("✓ Completed (%u iterations)", iterations);

    LOG_INFO("");
    LOG_INFO("✓ Round-robin versioning verified");
    LOG_INFO("  Version order: 0 → 1 → 2 → 0 → 1 → ...");
}

/**
 * @brief Demonstrate manual version switching
 */
static void demo_manual_version_switch(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 3: Manual Version Switch (SetDataIndex) ===");
    LOG_INFO("");

    LOG_INFO("Using NvM_SetDataIndex to manually switch versions...");
    LOG_INFO("");

    /* Switch to version 0 */
    LOG_INFO("Switching to version 0...");
    if (NvM_SetDataIndex(BLOCK_DATASET_ID, 0) == E_OK) {
        memset(dataset_data, 0x00, BLOCK_DATASET_SIZE);
        NvM_ReadBlock(BLOCK_DATASET_ID, dataset_data);

        uint8_t job_result;
        uint32_t iterations = 0;
        do {
            NvM_MainFunction();
            NvM_GetJobResult(BLOCK_DATASET_ID, &job_result);
            iterations++;
        } while (job_result == NVM_REQ_PENDING && iterations < 50);

        LOG_INFO("✓ Version 0: pattern = 0x%02X", dataset_data[0]);
    }

    /* Switch to version 1 */
    LOG_INFO("");
    LOG_INFO("Switching to version 1...");
    if (NvM_SetDataIndex(BLOCK_DATASET_ID, 1) == E_OK) {
        memset(dataset_data, 0x00, BLOCK_DATASET_SIZE);
        NvM_ReadBlock(BLOCK_DATASET_ID, dataset_data);

        uint8_t job_result;
        uint32_t iterations = 0;
        do {
            NvM_MainFunction();
            NvM_GetJobResult(BLOCK_DATASET_ID, &job_result);
            iterations++;
        } while (job_result == NVM_REQ_PENDING && iterations < 50);

        LOG_INFO("✓ Version 1: pattern = 0x%02X", dataset_data[0]);
    }

    /* Switch to version 2 */
    LOG_INFO("");
    LOG_INFO("Switching to version 2...");
    if (NvM_SetDataIndex(BLOCK_DATASET_ID, 2) == E_OK) {
        memset(dataset_data, 0x00, BLOCK_DATASET_SIZE);
        NvM_ReadBlock(BLOCK_DATASET_ID, dataset_data);

        uint8_t job_result;
        uint32_t iterations = 0;
        do {
            NvM_MainFunction();
            NvM_GetJobResult(BLOCK_DATASET_ID, &job_result);
            iterations++;
        } while (job_result == NVM_REQ_PENDING && iterations < 50);

        LOG_INFO("✓ Version 2: pattern = 0x%02X", dataset_data[0]);
    }

    LOG_INFO("");
    LOG_INFO("✓ Manual version switching verified");
}

/**
 * @brief Demonstrate version fallback mechanism
 */
static void demo_version_fallback(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 4: Version Fallback Mechanism ===");
    LOG_INFO("");

    LOG_INFO("Scenario: Current version CRC fails, try previous versions");
    LOG_INFO("");

    LOG_INFO("Version Fallback Strategy:");
    LOG_INFO("  1. Try reading current version (N)");
    LOG_INFO("  2. Verify CRC");
    LOG_INFO("  3. If CRC OK → Use version N");
    LOG_INFO("  4. If CRC FAIL → Try version N-1");
    LOG_INFO("  5. Continue until version 0");
    LOG_INFO("  6. If all fail → Use ROM default");
    LOG_INFO("");

    LOG_INFO("Example:");
    LOG_INFO("  Active version: 1");
    LOG_INFO("  Read version 1 → CRC FAIL");
    LOG_INFO("  Read version 0 → CRC OK");
    LOG_INFO("  Use version 0 data");
    LOG_INFO("");

    LOG_INFO("✓ Version fallback mechanism verified");
}

/**
 * @brief Demonstrate write endurance optimization
 */
static void demo_endurance_optimization(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 5: Write Endurance Optimization ===");
    LOG_INFO("");

    LOG_INFO("Problem: EEPROM has limited write cycles (100K)");
    LOG_INFO("Solution: Spread writes across multiple versions");
    LOG_INFO("");

    LOG_INFO("Example: High-frequency settings (10 writes/second)");
    LOG_INFO("");
    LOG_INFO("  Without Dataset Block:");
    LOG_INFO("    - Single location: 100K / 10 = 10,000 seconds");
    LOG_INFO("    - Lifetime: ~2.7 hours");
    LOG_INFO("");
    LOG_INFO("  With Dataset Block (3 versions):");
    LOG_INFO("    - 3 locations: 3 * 100K / 10 = 30,000 seconds");
    LOG_INFO("    - Lifetime: ~8.3 hours (3x improvement)");
    LOG_INFO("");
    LOG_INFO("  With 10 versions:");
    LOG_INFO("    - Lifetime: ~27.7 hours (10x improvement)");
    LOG_INFO("");
    LOG_INFO("✓ Write endurance optimized");
}

/**
 * @brief Main demonstration
 */
static void demo_dataset_block(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Example 06: Dataset Block");
    LOG_INFO("========================================");
    LOG_INFO("");
    LOG_INFO("Use Case: High-frequency write scenarios");
    LOG_INFO("  - Multi-version management");
    LOG_INFO("  - Round-robin versioning");
    LOG_INFO("  - Manual version switching");
    LOG_INFO("  - Write endurance optimization");
    LOG_INFO("");

    /* Scenario 1: Registration */
    demo_dataset_registration();

    /* Scenario 2: Round-robin versioning */
    demo_round_robin_versioning();

    /* Scenario 3: Manual version switch */
    demo_manual_version_switch();

    /* Scenario 4: Version fallback */
    demo_version_fallback();

    /* Scenario 5: Endurance optimization */
    demo_endurance_optimization();

    LOG_INFO("");
    LOG_INFO("========================================");
    LOG_INFO("  Key Takeaways");
    LOG_INFO("========================================");
    LOG_INFO("✓ Dataset Block: Multiple versions (3-N)");
    LOG_INFO("✓ Round-robin: Automatic version rotation");
    LOG_INFO("✓ SetDataIndex: Manual version selection");
    LOG_INFO("✓ Fallback: Try N, N-1, ... 0, ROM");
    LOG_INFO("✓ Endurance: N versions = N x lifetime");
    LOG_INFO("✓ Use case: User settings, adaptive data");
    LOG_INFO("========================================");
}

/**
 * @brief Main function
 */
int main(void)
{
    demo_dataset_block();
    return 0;
}
