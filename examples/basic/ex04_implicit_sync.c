/**
 * @file ex04_implicit_sync.c
 * @brief Example 04: Implicit Synchronization
 *
 * Learning Objectives:
 * - Understand ReadAll (startup consistency)
 * - Understand WriteAll (shutdown safety)
 * - Understand system-managed synchronization
 *
 * Design Reference: 05-使用示例与最佳实践.md §1.1
 *
 * Use Case: System automatically manages all blocks
 * - Startup: ReadAll loads all blocks
 * - Runtime: Application modifies RAM
 * - Shutdown: WriteAll saves all blocks
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

/* Multiple blocks for system-wide management */
#define BLOCK_SYS_CFG_ID   10
#define BLOCK_USER_CFG_ID  11
#define BLOCK_DIAG_ID      12
#define BLOCK_SIZE         256

/* RAM mirrors */
static uint8_t sys_config[BLOCK_SIZE];
static uint8_t user_config[BLOCK_SIZE];
static uint8_t diag_data[BLOCK_SIZE];

/* ROM defaults */
static const uint8_t rom_sys[BLOCK_SIZE] = { ['s'] = 1, [1 ... 255] = 0xFF };
static const uint8_t rom_user[BLOCK_SIZE] = { ['u'] = 2, [1 ... 255] = 0xFF };
static const uint8_t rom_diag[BLOCK_SIZE] = { [0 ... 255] = 0x00 };

/**
 * @brief Register multiple blocks for system-wide management
 */
static void register_system_blocks(void)
{
    /* System configuration block */
    NvM_BlockConfig_t sys_block = {
        .block_id = BLOCK_SYS_CFG_ID,
        .block_size = BLOCK_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 5,  /* High priority */
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = sys_config,
        .rom_block_ptr = rom_sys,
        .rom_block_size = sizeof(rom_sys),
        .eeprom_offset = 0x1000
    };
    NvM_RegisterBlock(&sys_block);

    /* User configuration block */
    NvM_BlockConfig_t user_block = {
        .block_id = BLOCK_USER_CFG_ID,
        .block_size = BLOCK_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = user_config,
        .rom_block_ptr = rom_user,
        .rom_block_size = sizeof(rom_user),
        .eeprom_offset = 0x1400
    };
    NvM_RegisterBlock(&user_block);

    /* Diagnostic data block */
    NvM_BlockConfig_t diag_block = {
        .block_id = BLOCK_DIAG_ID,
        .block_size = BLOCK_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 15,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = diag_data,
        .rom_block_ptr = rom_diag,
        .rom_block_size = sizeof(rom_diag),
        .eeprom_offset = 0x1800
    };
    NvM_RegisterBlock(&diag_block);

    LOG_INFO("✓ 3 blocks registered (SYS, USER, DIAG)");
}

/**
 * @brief Demonstrate ReadAll (startup consistency)
 */
static void demo_readall_startup(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 1: System Startup (ReadAll) ===");
    LOG_INFO("");

    LOG_INFO("[System Boot]");
    LOG_INFO("Initializing NvM and loading all blocks...");

    NvM_Init();
    OsScheduler_Init(16);
    register_system_blocks();

    LOG_INFO("");
    LOG_INFO("Triggering ReadAll (loads all 3 blocks)...");
    NvM_ReadAll();

    /* Process jobs until all complete */
    uint32_t total_iterations = 0;
    uint8_t job_result;
    boolean all_done;

    do {
        NvM_MainFunction();
        total_iterations++;

        /* Check if all blocks are done */
        all_done = TRUE;
        for (int id = BLOCK_SYS_CFG_ID; id <= BLOCK_DIAG_ID; id++) {
            NvM_GetJobResult(id, &job_result);
            if (job_result == NVM_REQ_PENDING) {
                all_done = FALSE;
                break;
            }
        }

        if (total_iterations % 20 == 0) {
            LOG_INFO("  Processing... (iteration %u)", total_iterations);
        }

    } while (!all_done && total_iterations < 500);

    LOG_INFO("");
    LOG_INFO("✓ ReadAll completed (%u iterations)", total_iterations);

    /* Display loaded data */
    LOG_INFO("  [SYS_CONFIG]  Marker: 0x%02X", sys_config[0]);
    LOG_INFO("  [USER_CONFIG] Marker: 0x%02X", user_config[0]);
    LOG_INFO("  [DIAG_DATA]  Marker: 0x%02X", diag_data[0]);
    LOG_INFO("");

    if (sys_config[0] == 's' && user_config[0] == 'u') {
        LOG_INFO("✓ All blocks loaded from ROM defaults");
    } else {
        LOG_ERROR("✗ Block loading FAILED");
    }
}

/**
 * @brief Demonstrate runtime modifications
 */
static void demo_runtime_modifications(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 2: Runtime Modifications ===");
    LOG_INFO("");

    LOG_INFO("[Application Running]");
    LOG_INFO("Modifying RAM mirrors (not yet saved to EEPROM)...");
    LOG_INFO("");

    /* Modify system config */
    sys_config['s'] = 10;
    sys_config['v'] = 100;  /* Set version */
    LOG_INFO("  SYS_CONFIG: Modified (version=100)");

    /* Modify user config */
    user_config['u'] = 20;
    user_config['p'] = 1;   /* Set profile */
    LOG_INFO("  USER_CONFIG: Modified (profile=1)");

    /* Modify diagnostic data */
    diag_data['e'] = 1;     /* Set error flag */
    diag_data['c'] = 5;     /* Error code */
    LOG_INFO("  DIAG_DATA: Modified (error=5)");

    LOG_INFO("");
    LOG_INFO("Note: Changes are in RAM only (DIRTY)");
    LOG_INFO("      Not yet persisted to EEPROM");
}

/**
 * @brief Demonstrate WriteAll (shutdown safety)
 */
static void demo_writeall_shutdown(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 3: System Shutdown (WriteAll) ===");
    LOG_INFO("");

    LOG_INFO("[Shutdown Signal Received]");
    LOG_INFO("Saving all blocks to EEPROM...");
    LOG_INFO("");

    /* Trigger WriteAll */
    LOG_INFO("Triggering WriteAll (saves all 3 blocks)...");
    NvM_WriteAll();

    /* Process jobs until all complete */
    uint32_t total_iterations = 0;
    uint8_t job_result;
    boolean all_done;

    do {
        NvM_MainFunction();
        total_iterations++;

        /* Check if all blocks are done */
        all_done = TRUE;
        for (int id = BLOCK_SYS_CFG_ID; id <= BLOCK_DIAG_ID; id++) {
            NvM_GetJobResult(id, &job_result);
            if (job_result == NVM_REQ_PENDING) {
                all_done = FALSE;
                break;
            }
        }

        if (total_iterations % 20 == 0) {
            LOG_INFO("  Writing... (iteration %u)", total_iterations);
        }

    } while (!all_done && total_iterations < 500);

    LOG_INFO("");
    LOG_INFO("✓ WriteAll completed (%u iterations)", total_iterations);

    /* Verify all writes succeeded */
    LOG_INFO("");
    LOG_INFO("Verifying writes...");
    boolean all_ok = TRUE;
    for (int id = BLOCK_SYS_CFG_ID; id <= BLOCK_DIAG_ID; id++) {
        NvM_GetJobResult(id, &job_result);
        if (job_result != NVM_REQ_OK) {
            LOG_ERROR("  ✗ Block %d failed (result=%u)", id, job_result);
            all_ok = FALSE;
        } else {
            LOG_INFO("  ✓ Block %d saved successfully", id);
        }
    }

    if (all_ok) {
        LOG_INFO("");
        LOG_INFO("✓ All blocks safely persisted");
        LOG_INFO("  System can now shutdown safely");
    }
}

/**
 * @brief Main demonstration
 */
static void demo_implicit_sync(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Example 04: Implicit Synchronization");
    LOG_INFO("========================================");
    LOG_INFO("");
    LOG_INFO("Use Case: System-managed synchronization");
    LOG_INFO("  - Startup: ReadAll loads all blocks");
    LOG_INFO("  - Runtime: Application modifies RAM");
    LOG_INFO("  - Shutdown: WriteAll saves all blocks");
    LOG_INFO("");

    /* Scenario 1: Startup */
    demo_readall_startup();

    /* Scenario 2: Runtime */
    demo_runtime_modifications();

    /* Scenario 3: Shutdown */
    demo_writeall_shutdown();

    LOG_INFO("");
    LOG_INFO("========================================");
    LOG_INFO("  Key Takeaways");
    LOG_INFO("========================================");
    LOG_INFO("✓ ReadAll: Automatic startup consistency");
    LOG_INFO("✓ WriteAll: Automatic shutdown safety");
    LOG_INFO("✓ Implicit: System manages timing");
    LOG_INFO("✓ Benefit: No manual sync tracking needed");
    LOG_INFO("========================================");
}

/**
 * @brief Main function
 */
int main(void)
{
    demo_implicit_sync();
    return 0;
}
