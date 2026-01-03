/**
 * @file ex10_power_cycle.c
 * @brief Example 10: Power Cycle and Recovery
 *
 * Learning Objectives:
 * - Understand ROM fallback mechanism
 * - Understand WriteAll two-phase commit
 * - Understand power loss during write
 * - Understand data recovery strategies
 *
 * Design Reference: 04-数据完整性方案.md §3.3
 *
 * Use Case: System power cycle recovery
 * - Startup: ReadAll with consistency check
 * - Runtime: Normal read/write operations
 * - Shutdown: WriteAll with safe persistence
 * - Recovery: Power loss handling
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

/* Block configurations */
#define BLOCK_SETTINGS_ID   130
#define BLOCK_COUNTER_ID    131
#define BLOCK_CRC_ID        132
#define BLOCK_SIZE          256

/* RAM mirrors */
static uint8_t settings_data[BLOCK_SIZE];
static uint8_t counter_data[BLOCK_SIZE];
static uint8_t crc_data[BLOCK_SIZE];

/* ROM defaults */
static const uint8_t rom_settings[BLOCK_SIZE] = {
    ['v'] = 1, ['m'] = 0, [2 ... 255] = 0xFF
};

/**
 * @brief Register blocks with different recovery strategies
 */
static void register_recovery_blocks(void)
{
    /* Settings with ROM fallback */
    NvM_BlockConfig_t settings_block = {
        .block_id = BLOCK_SETTINGS_ID,
        .block_size = BLOCK_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 5,  /* High priority */
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = settings_data,
        .rom_block_ptr = rom_settings,
        .rom_block_size = sizeof(rom_settings),
        .eeprom_offset = 0x8000
    };
    NvM_RegisterBlock(&settings_block);
    LOG_INFO("✓ Block %d registered (ROM fallback)", BLOCK_SETTINGS_ID);

    /* Counter without ROM (volatile data) */
    NvM_BlockConfig_t counter_block = {
        .block_id = BLOCK_COUNTER_ID,
        .block_size = BLOCK_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = counter_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x8400
    };
    NvM_RegisterBlock(&counter_block);
    LOG_INFO("✓ Block %d registered (no ROM)", BLOCK_COUNTER_ID);

    /* CRC-protected block */
    NvM_BlockConfig_t crc_block = {
        .block_id = BLOCK_CRC_ID,
        .block_size = BLOCK_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC32,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = crc_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x8800
    };
    NvM_RegisterBlock(&crc_block);
    LOG_INFO("✓ Block %d registered (CRC32)", BLOCK_CRC_ID);
}

/**
 * @brief Demonstrate normal startup sequence
 */
static void demo_normal_startup(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 1: Normal Startup (ReadAll) ===");
    LOG_INFO("");

    LOG_INFO("[System Boot]");
    LOG_INFO("Step 1: Initialize NvM...");
    NvM_Init();

    LOG_INFO("Step 2: Register blocks...");
    OsScheduler_Init(16);
    register_recovery_blocks();

    LOG_INFO("Step 3: Trigger ReadAll (load all blocks)...");
    NvM_ReadAll();

    /* Process all read jobs */
    uint32_t iterations = 0;
    uint8_t results[3];
    uint16_t ids[3] = {BLOCK_SETTINGS_ID, BLOCK_COUNTER_ID, BLOCK_CRC_ID};
    boolean all_done;

    do {
        NvM_MainFunction();
        iterations++;

        all_done = TRUE;
        for (int i = 0; i < 3; i++) {
            if (results[i] == NVM_REQ_PENDING || results[i] == NVM_REQ_BLOCK_SKIPPED) {
                NvM_GetJobResult(ids[i], &results[i]);
            }
            if (results[i] == NVM_REQ_PENDING) {
                all_done = FALSE;
            }
        }

        if (iterations % 20 == 0) {
            LOG_INFO("  Loading... (iteration %u)", iterations);
        }

    } while (!all_done && iterations < 500);

    LOG_INFO("");
    LOG_INFO("✓ ReadAll completed (%u iterations)", iterations);

    /* Display loaded data */
    LOG_INFO("");
    LOG_INFO("Loaded Data:");
    LOG_INFO("  Block %d (ROM fallback):", BLOCK_SETTINGS_ID);
    LOG_INFO("    Marker: 0x%02X (expected: 'v'=0x76 or ROM default)", settings_data[0]);
    LOG_INFO("    Mode: %u", settings_data['m']);
    LOG_INFO("");
    LOG_INFO("  Block %d (no ROM):", BLOCK_COUNTER_ID);
    LOG_INFO("    First byte: 0x%02X", counter_data[0]);
    LOG_INFO("    (Uninitialized or 0x00 if EEPROM empty)");
    LOG_INFO("");
    LOG_INFO("  Block %d (CRC32):", BLOCK_CRC_ID);
    LOG_INFO("    First byte: 0x%02X", crc_data[0]);
}

/**
 * @brief Demonstrate runtime data modifications
 */
static void demo_runtime_operations(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 2: Runtime Operations ===");
    LOG_INFO("");

    LOG_INFO("[Application Running]");
    LOG_INFO("Modifying data in RAM mirrors...");
    LOG_INFO("");

    /* Modify settings */
    settings_data['v'] = 2;        /* Update version */
    settings_data['m'] = 1;        /* Change mode */
    settings_data['s'] = 100;      /* Add setting */
    LOG_INFO("Settings modified:");
    LOG_INFO("  Version: 2");
    LOG_INFO("  Mode: 1");
    LOG_INFO("  Setting: 100");
    LOG_INFO("");

    /* Modify counter */
    uint32_t* counter_ptr = (uint32_t*)counter_data;
    *counter_ptr = 12345;
    LOG_INFO("Counter modified:");
    LOG_INFO("  Value: 12345");
    LOG_INFO("");

    /* Modify CRC block */
    memset(crc_data, 0xAA, BLOCK_SIZE);
    LOG_INFO("CRC block modified:");
    LOG_INFO("  Pattern: 0xAA");
    LOG_INFO("");

    LOG_INFO("Note: All changes are in RAM only (DIRTY)");
    LOG_INFO("      Not yet persisted to EEPROM");
}

/**
 * @brief Demonstrate normal shutdown sequence
 */
static void demo_normal_shutdown(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 3: Normal Shutdown (WriteAll) ===");
    LOG_INFO("");

    LOG_INFO("[Shutdown Signal]");
    LOG_INFO("Saving all blocks to EEPROM...");
    LOG_INFO("");

    LOG_INFO("Step 1: Trigger WriteAll...");
    NvM_WriteAll();

    /* Process all write jobs */
    uint32_t iterations = 0;
    uint8_t results[3];
    uint16_t ids[3] = {BLOCK_SETTINGS_ID, BLOCK_COUNTER_ID, BLOCK_CRC_ID};
    boolean all_done;

    do {
        NvM_MainFunction();
        iterations++;

        all_done = TRUE;
        for (int i = 0; i < 3; i++) {
            if (results[i] == NVM_REQ_PENDING) {
                NvM_GetJobResult(ids[i], &results[i]);
            }
            if (results[i] == NVM_REQ_PENDING) {
                all_done = FALSE;
            }
        }

        if (iterations % 20 == 0) {
            LOG_INFO("  Writing... (iteration %u)", iterations);
        }

    } while (!all_done && iterations < 500);

    LOG_INFO("");
    LOG_INFO("✓ WriteAll completed (%u iterations)", iterations);

    /* Verify all writes */
    LOG_INFO("");
    LOG_INFO("Verification:");
    boolean all_ok = TRUE;
    for (int i = 0; i < 3; i++) {
        if (results[i] != NVM_REQ_OK) {
            LOG_ERROR("  ✗ Block %u failed (result=%u)", ids[i], results[i]);
            all_ok = FALSE;
        } else {
            LOG_INFO("  ✓ Block %u saved successfully", ids[i]);
        }
    }

    if (all_ok) {
        LOG_INFO("");
        LOG_INFO("✓ All blocks safely persisted");
        LOG_INFO("  System can now power off safely");
    }
}

/**
 * @brief Demonstrate power loss recovery
 */
static void demo_power_loss_recovery(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 4: Power Loss Recovery ===");
    LOG_INFO("");

    LOG_INFO("Simulating power loss during write...");
    LOG_INFO("");

    /* Re-initialize (simulating power cycle) */
    NvM_Init();
    NvM_BlockConfig_t settings_block = {
        .block_id = BLOCK_SETTINGS_ID,
        .block_size = BLOCK_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 5,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = settings_data,
        .rom_block_ptr = rom_settings,
        .rom_block_size = sizeof(rom_settings),
        .eeprom_offset = 0x8000
    };
    NvM_RegisterBlock(&settings_block);

    LOG_INFO("[Power Cycle Detected]");
    LOG_INFO("Recovery strategy:");
    LOG_INFO("");
    LOG_INFO("  1. Check EEPROM data integrity (CRC)");
    LOG_INFO("  2. If CRC OK → Use EEPROM data");
    LOG_INFO("  3. If CRC FAIL → Use ROM default");
    LOG_INFO("  4. Mark block as consistent");
    LOG_INFO("");

    /* Read with automatic recovery */
    memset(settings_data, 0x00, BLOCK_SIZE);
    LOG_INFO("Reading block with automatic recovery...");
    NvM_ReadBlock(BLOCK_SETTINGS_ID, settings_data);

    uint8_t job_result;
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(BLOCK_SETTINGS_ID, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    LOG_INFO("");
    LOG_INFO("✓ Recovery completed (%u iterations)", iterations);

    /* Check recovery result */
    LOG_INFO("");
    LOG_INFO("Recovery Result:");
    if (settings_data[0] == 'v') {
        LOG_INFO("  ✓ Data recovered from EEPROM");
        LOG_INFO("    Version: %u", settings_data['v']);
        LOG_INFO("    Mode: %u", settings_data['m']);
    } else if (settings_data[0] == rom_settings[0]) {
        LOG_INFO("  ✓ Data recovered from ROM default");
        LOG_INFO("    (EEPROM data was corrupted)");
    } else {
        LOG_ERROR("  ✗ Recovery failed");
        LOG_ERROR("    Data: 0x%02X", settings_data[0]);
    }
}

/**
 * @brief Demonstrate recovery strategies
 */
static void demo_recovery_strategies(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 5: Recovery Strategy Comparison ===");
    LOG_INFO("");

    LOG_INFO("Recovery Strategy Hierarchy:");
    LOG_INFO("");
    LOG_INFO("  Level 1: EEPROM Data (Primary)");
    LOG_INFO("    - Check CRC");
    LOG_INFO("    - If valid → Use EEPROM");
    LOG_INFO("");
    LOG_INFO("  Level 2: ROM Default (Fallback)");
    LOG_INFO("    - Used when EEPROM CRC fails");
    LOG_INFO("    - Factory-programmed safe values");
    LOG_INFO("    - Example: VIN, default settings");
    LOG_INFO("");
    LOG_INFO("  Level 3: Redundant Copy (Backup)");
    LOG_INFO("    - For REDUNDANT block type");
    LOG_INFO("    - Primary fail → Try backup");
    LOG_INFO("    - Both fail → Use ROM");
    LOG_INFO("");
    LOG_INFO("  Level 4: Dataset Versions (Rollback)");
    LOG_INFO("    - For DATASET block type");
    LOG_INFO("    - Try version N-1, N-2...");
    LOG_INFO("    - All fail → Use ROM");
    LOG_INFO("");
}

/**
 * @brief Main demonstration
 */
static void demo_power_cycle(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Example 10: Power Cycle & Recovery");
    LOG_INFO("========================================");
    LOG_INFO("");
    LOG_INFO("Use Case: System power cycle recovery");
    LOG_INFO("  - Startup: ReadAll with consistency");
    LOG_INFO("  - Runtime: Normal operations");
    LOG_INFO("  - Shutdown: WriteAll safely");
    LOG_INFO("  - Recovery: Power loss handling");
    LOG_INFO("");

    /* Scenario 1: Normal startup */
    demo_normal_startup();

    /* Scenario 2: Runtime operations */
    demo_runtime_operations();

    /* Scenario 3: Normal shutdown */
    demo_normal_shutdown();

    /* Scenario 4: Power loss recovery */
    demo_power_loss_recovery();

    /* Scenario 5: Recovery strategies */
    demo_recovery_strategies();

    LOG_INFO("");
    LOG_INFO("========================================");
    LOG_INFO("  Key Takeaways");
    LOG_INFO("========================================");
    LOG_INFO("✓ ReadAll: Automatic startup consistency");
    LOG_INFO("✓ WriteAll: Safe shutdown persistence");
    LOG_INFO("✓ Recovery: EEPROM > ROM > Redundant > Dataset");
    LOG_INFO("✓ CRC: Detect corrupted data");
    LOG_INFO("✓ ROM fallback: Factory defaults always available");
    LOG_INFO("========================================");
}

/**
 * @brief Main function
 */
int main(void)
{
    demo_power_cycle();
    return 0;
}
