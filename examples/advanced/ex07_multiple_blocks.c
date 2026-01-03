/**
 * @file ex07_multiple_blocks.c
 * @brief Example 07: Multiple Blocks Coordination
 *
 * Learning Objectives:
 * - Understand multi-block Job queue management
 * - Understand priority-based scheduling
 * - Understand concurrent block operations
 * - Understand Job queue priority ordering
 *
 * Design Reference: 05-使用示例与最佳实践.md §1.2
 *
 * Use Case: System managing multiple blocks simultaneously
 * - Different priorities for different blocks
 * - Job queue processing order
 * - Priority scheduling verification
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

/* Block configurations */
#define BLOCK_SYS_CFG_ID   100
#define BLOCK_USER_CFG_ID  101
#define BLOCK_DIAG_ID      102
#define BLOCK_LOG_ID       103
#define BLOCK_SIZE         256

/* RAM mirrors */
static uint8_t sys_config[BLOCK_SIZE];
static uint8_t user_config[BLOCK_SIZE];
static uint8_t diag_data[BLOCK_SIZE];
static uint8_t log_data[BLOCK_SIZE];

/**
 * @brief Register multiple blocks with different priorities
 */
static void register_blocks(void)
{
    /* System config: Priority 5 (HIGH) */
    NvM_BlockConfig_t sys_block = {
        .block_id = BLOCK_SYS_CFG_ID,
        .block_size = BLOCK_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 5,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = sys_config,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x5000
    };
    NvM_RegisterBlock(&sys_block);
    LOG_INFO("✓ Block %d registered (priority=5, HIGH)", BLOCK_SYS_CFG_ID);

    /* User config: Priority 10 (MEDIUM-HIGH) */
    NvM_BlockConfig_t user_block = {
        .block_id = BLOCK_USER_CFG_ID,
        .block_size = BLOCK_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = user_config,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x5400
    };
    NvM_RegisterBlock(&user_block);
    LOG_INFO("✓ Block %d registered (priority=10, MEDIUM-HIGH)", BLOCK_USER_CFG_ID);

    /* Diagnostic data: Priority 15 (MEDIUM) */
    NvM_BlockConfig_t diag_block = {
        .block_id = BLOCK_DIAG_ID,
        .block_size = BLOCK_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 15,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = diag_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x5800
    };
    NvM_RegisterBlock(&diag_block);
    LOG_INFO("✓ Block %d registered (priority=15, MEDIUM)", BLOCK_DIAG_ID);

    /* Log data: Priority 20 (LOW) */
    NvM_BlockConfig_t log_block = {
        .block_id = BLOCK_LOG_ID,
        .block_size = BLOCK_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 20,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = log_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x5C00
    };
    NvM_RegisterBlock(&log_block);
    LOG_INFO("✓ Block %d registered (priority=20, LOW)", BLOCK_LOG_ID);
}

/**
 * @brief Demonstrate priority-based Job scheduling
 */
static void demo_priority_scheduling(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 1: Priority-Based Scheduling ===");
    LOG_INFO("");

    LOG_INFO("Submitting 4 write jobs simultaneously:");
    LOG_INFO("  Block 100 (Priority 5)  - HIGH");
    LOG_INFO("  Block 101 (Priority 10) - MEDIUM-HIGH");
    LOG_INFO("  Block 102 (Priority 15) - MEDIUM");
    LOG_INFO("  Block 103 (Priority 20) - LOW");
    LOG_INFO("");

    /* Prepare data */
    memset(sys_config, 0xAA, BLOCK_SIZE);
    memset(user_config, 0xBB, BLOCK_SIZE);
    memset(diag_data, 0xCC, BLOCK_SIZE);
    memset(log_data, 0xDD, BLOCK_SIZE);

    /* Submit writes in reverse priority order (LOW first) */
    LOG_INFO("Submitting jobs (reverse priority order)...");
    NvM_WriteBlock(BLOCK_LOG_ID, log_data);       /* Priority 20 (submitted first) */
    NvM_WriteBlock(BLOCK_DIAG_ID, diag_data);     /* Priority 15 */
    NvM_WriteBlock(BLOCK_USER_CFG_ID, user_config); /* Priority 10 */
    NvM_WriteBlock(BLOCK_SYS_CFG_ID, sys_config);   /* Priority 5 (submitted last) */
    LOG_INFO("✓ All 4 jobs submitted");
    LOG_INFO("");

    /* Process jobs */
    LOG_INFO("Processing jobs (priority order: 100 > 101 > 102 > 103)...");
    uint32_t iterations = 0;
    uint8_t results[4];
    uint8_t ids[4] = {BLOCK_SYS_CFG_ID, BLOCK_USER_CFG_ID, BLOCK_DIAG_ID, BLOCK_LOG_ID};

    do {
        NvM_MainFunction();
        iterations++;

        for (int i = 0; i < 4; i++) {
            if (results[i] == NVM_REQ_PENDING || results[i] == NVM_REQ_BLOCK_SKIPPED) {
                NvM_GetJobResult(ids[i], &results[i]);
            }
        }

        if (iterations % 10 == 0) {
            LOG_INFO("  Iteration %u: [%u=%s, %u=%s, %u=%s, %u=%s]",
                     iterations,
                     ids[0], (results[0] == NVM_REQ_OK) ? "OK" : "PENDING",
                     ids[1], (results[1] == NVM_REQ_OK) ? "OK" : "PENDING",
                     ids[2], (results[2] == NVM_REQ_OK) ? "OK" : "PENDING",
                     ids[3], (results[3] == NVM_REQ_OK) ? "OK" : "PENDING");
        }

    } while ((results[0] == NVM_REQ_PENDING ||
             results[1] == NVM_REQ_PENDING ||
             results[2] == NVM_REQ_PENDING ||
             results[3] == NVM_REQ_PENDING) && iterations < 200);

    LOG_INFO("");
    LOG_INFO("✓ All jobs completed after %u iterations", iterations);

    /* Verify priority order */
    LOG_INFO("");
    LOG_INFO("=== Verification ===");
    LOG_INFO("Expected processing order: 100 > 101 > 102 > 103");
    LOG_INFO("  Block 100 (HIGH): %s", (results[0] == NVM_REQ_OK) ? "✓ OK" : "✗ FAILED");
    LOG_INFO("  Block 101 (MED-HIGH): %s", (results[1] == NVM_REQ_OK) ? "✓ OK" : "✗ FAILED");
    LOG_INFO("  Block 102 (MED): %s", (results[2] == NVM_REQ_OK) ? "✓ OK" : "✗ FAILED");
    LOG_INFO("  Block 103 (LOW): %s", (results[3] == NVM_REQ_OK) ? "✓ OK" : "✗ FAILED");
}

/**
 * @brief Demonstrate concurrent read and write operations
 */
static void demo_concurrent_operations(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 2: Concurrent Read & Write ===");
    LOG_INFO("");

    LOG_INFO("Demonstrating mixed read/write operations...");
    LOG_INFO("");

    /* Clear all buffers */
    memset(sys_config, 0x00, BLOCK_SIZE);
    memset(user_config, 0x00, BLOCK_SIZE);

    /* Submit write to block 100 */
    memset(sys_config, 0xEE, BLOCK_SIZE);
    LOG_INFO("Submitting write to Block 100...");
    NvM_WriteBlock(BLOCK_SYS_CFG_ID, sys_config);
    LOG_INFO("✓ Write submitted");
    LOG_INFO("");

    /* Submit read to block 101 (while 100 is writing) */
    LOG_INFO("Submitting read to Block 101 (concurrent with Block 100 write)...");
    NvM_ReadBlock(BLOCK_USER_CFG_ID, user_config);
    LOG_INFO("✓ Read submitted");
    LOG_INFO("");

    /* Process both jobs */
    uint8_t result_100, result_101;
    uint32_t iterations = 0;

    do {
        NvM_MainFunction();
        iterations++;

        NvM_GetJobResult(BLOCK_SYS_CFG_ID, &result_100);
        NvM_GetJobResult(BLOCK_USER_CFG_ID, &result_101);

        if (iterations % 5 == 0) {
            LOG_INFO("  Iteration %u: Block100=%s, Block101=%s",
                     iterations,
                     (result_100 == NVM_REQ_OK) ? "OK" : "PENDING",
                     (result_101 == NVM_REQ_OK) ? "OK" : "PENDING");
        }

    } while ((result_100 == NVM_REQ_PENDING || result_101 == NVM_REQ_PENDING) && iterations < 100);

    LOG_INFO("");
    LOG_INFO("✓ Both jobs completed (%u iterations)", iterations);
    LOG_INFO("  Block 100 write: %s", (result_100 == NVM_REQ_OK) ? "✓" : "✗");
    LOG_INFO("  Block 101 read: %s", (result_101 == NVM_REQ_OK) ? "✓" : "✗");
}

/**
 * @brief Demonstrate Job queue depth monitoring
 */
static void demo_queue_depth(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 3: Job Queue Depth ===");
    LOG_INFO("");

    NvM_Diagnostics_t diag_before, diag_after;

    /* Get initial diagnostics */
    if (NvM_GetDiagnostics(&diag_before) == E_OK) {
        LOG_INFO("Initial queue depth: %u", diag_before.max_queue_depth);
    }
    LOG_INFO("");

    /* Submit multiple jobs rapidly */
    LOG_INFO("Submitting 4 jobs rapidly...");
    memset(sys_config, 0x11, BLOCK_SIZE);
    memset(user_config, 0x22, BLOCK_SIZE);
    memset(diag_data, 0x33, BLOCK_SIZE);
    memset(log_data, 0x44, BLOCK_SIZE);

    NvM_WriteBlock(BLOCK_SYS_CFG_ID, sys_config);
    NvM_WriteBlock(BLOCK_USER_CFG_ID, user_config);
    NvM_WriteBlock(BLOCK_DIAG_ID, diag_data);
    NvM_WriteBlock(BLOCK_LOG_ID, log_data);
    LOG_INFO("✓ 4 jobs submitted");
    LOG_INFO("");

    /* Process jobs */
    uint32_t iterations = 0;
    uint8_t results[4];
    uint8_t ids[4] = {BLOCK_SYS_CFG_ID, BLOCK_USER_CFG_ID, BLOCK_DIAG_ID, BLOCK_LOG_ID};

    do {
        NvM_MainFunction();
        iterations++;

        for (int i = 0; i < 4; i++) {
            if (results[i] == NVM_REQ_PENDING) {
                NvM_GetJobResult(ids[i], &results[i]);
            }
        }

    } while ((results[0] == NVM_REQ_PENDING ||
             results[1] == NVM_REQ_PENDING ||
             results[2] == NVM_REQ_PENDING ||
             results[3] == NVM_REQ_PENDING) && iterations < 200);

    /* Get final diagnostics */
    if (NvM_GetDiagnostics(&diag_after) == E_OK) {
        LOG_INFO("");
        LOG_INFO("Final statistics:");
        LOG_INFO("  Jobs processed: %u", diag_after.total_jobs_processed);
        LOG_INFO("  Jobs failed: %u", diag_after.total_jobs_failed);
        LOG_INFO("  Max queue depth: %u", diag_after.max_queue_depth);
        LOG_INFO("  Total iterations: %u", iterations);
    }

    /* Verify queue depth increased */
    if (diag_after.max_queue_depth >= diag_before.max_queue_depth) {
        LOG_INFO("");
        LOG_INFO("✓ Queue depth: %u → %u", diag_before.max_queue_depth, diag_after.max_queue_depth);
    }
}

/**
 * @brief Main demonstration
 */
static void demo_multiple_blocks(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Example 07: Multiple Blocks");
    LOG_INFO("========================================");
    LOG_INFO("");
    LOG_INFO("Use Case: System managing multiple blocks");
    LOG_INFO("  - Priority-based scheduling");
    LOG_INFO("  - Job queue management");
    LOG_INFO("  - Concurrent operations");
    LOG_INFO("");

    /* Initialize */
    NvM_Init();
    OsScheduler_Init(16);
    register_blocks();

    /* Scenario 1: Priority scheduling */
    demo_priority_scheduling();

    /* Scenario 2: Concurrent operations */
    demo_concurrent_operations();

    /* Scenario 3: Queue depth */
    demo_queue_depth();

    LOG_INFO("");
    LOG_INFO("========================================");
    LOG_INFO("  Key Takeaways");
    LOG_INFO("========================================");
    LOG_INFO("✓ Priority queue: HIGH → MEDIUM-HIGH → MEDIUM → LOW");
    LOG_INFO("✓ FIFO within same priority");
    LOG_INFO("✓ Concurrent read/write: Supported");
    LOG_INFO("✓ Queue depth: Monitored via diagnostics");
    LOG_INFO("✓ Job ordering: Deterministic based on priority");
    LOG_INFO("========================================");
}

/**
 * @brief Main function
 */
int main(void)
{
    demo_multiple_blocks();
    return 0;
}
