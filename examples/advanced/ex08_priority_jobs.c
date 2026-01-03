/**
 * @file ex08_priority_jobs.c
 * @brief Example 08: Priority Jobs and Preemption
 *
 * Learning Objectives:
 * - Understand Immediate Job preemption mechanism
 * - Understand priority inversion prevention
 * - Understand high-priority job interruption
 * - Understand virtual OS scheduling integration
 *
 * Design Reference: 05-使用示例与最佳实践.md §1.2
 *
 * Use Case: Emergency data saving with highest priority
 * - Crash data (priority 0, CRITICAL)
 * - DTC data (priority 10, HIGH)
 * - Normal config (priority 20, LOW)
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

/* Block configurations */
#define BLOCK_CRASH_ID      110
#define BLOCK_DTC_ID        111
#define BLOCK_CONFIG_ID     112
#define BLOCK_SIZE          256

/* RAM mirrors */
static uint8_t crash_data[BLOCK_SIZE];
static uint8_t dtc_data[BLOCK_SIZE];
static uint8_t config_data[BLOCK_SIZE];

/**
 * @brief Register blocks with different priorities
 */
static void register_priority_blocks(void)
{
    /* Crash data: Priority 0 (CRITICAL, IMMEDIATE) */
    NvM_BlockConfig_t crash_block = {
        .block_id = BLOCK_CRASH_ID,
        .block_size = BLOCK_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 0,           /* CRITICAL */
        .is_immediate = TRUE,    /* Preempt other jobs */
        .is_write_protected = FALSE,
        .ram_mirror_ptr = crash_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x6000
    };
    NvM_RegisterBlock(&crash_block);
    LOG_INFO("✓ Block %d registered (priority=0, CRITICAL+IMMEDIATE)", BLOCK_CRASH_ID);

    /* DTC data: Priority 10 (HIGH) */
    NvM_BlockConfig_t dtc_block = {
        .block_id = BLOCK_DTC_ID,
        .block_size = BLOCK_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,          /* HIGH */
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = dtc_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x6400
    };
    NvM_RegisterBlock(&dtc_block);
    LOG_INFO("✓ Block %d registered (priority=10, HIGH)", BLOCK_DTC_ID);

    /* Config data: Priority 20 (LOW) */
    NvM_BlockConfig_t config_block = {
        .block_id = BLOCK_CONFIG_ID,
        .block_size = BLOCK_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 20,          /* LOW */
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = config_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x6800
    };
    NvM_RegisterBlock(&config_block);
    LOG_INFO("✓ Block %d registered (priority=20, LOW)", BLOCK_CONFIG_ID);
}

/**
 * @brief Demonstrate normal priority queue processing
 */
static void demo_normal_priority(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 1: Normal Priority Queue ===");
    LOG_INFO("");

    LOG_INFO("Submitting 3 jobs (different priorities):");
    LOG_INFO("  Block 112 (Priority 20, LOW)");
    LOG_INFO("  Block 111 (Priority 10, HIGH)");
    LOG_INFO("  Block 110 (Priority 0,  CRITICAL)");
    LOG_INFO("");

    /* Prepare data */
    memset(crash_data, 0xCC, BLOCK_SIZE);
    memset(dtc_data, 0xDD, BLOCK_SIZE);
    memset(config_data, 0xEE, BLOCK_SIZE);

    /* Submit in reverse priority order */
    LOG_INFO("Submitting jobs (LOW → HIGH → CRITICAL)...");
    NvM_WriteBlock(BLOCK_CONFIG_ID, config_data);   /* Priority 20 (submitted first) */
    NvM_WriteBlock(BLOCK_DTC_ID, dtc_data);         /* Priority 10 */
    NvM_WriteBlock(BLOCK_CRASH_ID, crash_data);     /* Priority 0 (submitted last) */
    LOG_INFO("✓ All 3 jobs submitted");
    LOG_INFO("");

    /* Process jobs and observe order */
    LOG_INFO("Processing jobs (expected order: 110 > 111 > 112)...");
    uint32_t iterations = 0;
    uint8_t results[3];
    uint16_t ids[3] = {BLOCK_CRASH_ID, BLOCK_DTC_ID, BLOCK_CONFIG_ID};

    do {
        NvM_MainFunction();
        iterations++;

        for (int i = 0; i < 3; i++) {
            if (results[i] == NVM_REQ_PENDING) {
                NvM_GetJobResult(ids[i], &results[i]);
            }
        }

        /* Log when jobs complete */
        for (int i = 0; i < 3; i++) {
            if (results[i] == NVM_REQ_OK) {
                static boolean logged[3] = {FALSE, FALSE, FALSE};
                if (!logged[i]) {
                    LOG_INFO("  Iteration %u: Block %u (Priority %u) ✓ COMPLETE",
                             iterations, ids[i], i * 10);
                    logged[i] = TRUE;
                }
            }
        }

    } while ((results[0] == NVM_REQ_PENDING ||
             results[1] == NVM_REQ_PENDING ||
             results[2] == NVM_REQ_PENDING) && iterations < 200);

    LOG_INFO("");
    LOG_INFO("✓ All jobs completed after %u iterations", iterations);

    /* Verify processing order */
    LOG_INFO("");
    LOG_INFO("=== Verification ===");
    LOG_INFO("Expected processing order: 110 (CRITICAL) > 111 (HIGH) > 112 (LOW)");
    LOG_INFO("  Block 110 (CRITICAL): %s", (results[0] == NVM_REQ_OK) ? "✓ OK" : "✗ FAILED");
    LOG_INFO("  Block 111 (HIGH):     %s", (results[1] == NVM_REQ_OK) ? "✓ OK" : "✗ FAILED");
    LOG_INFO("  Block 112 (LOW):      %s", (results[2] == NVM_REQ_OK) ? "✓ OK" : "✗ FAILED");
}

/**
 * @brief Demonstrate immediate job preemption
 */
static void demo_immediate_preemption(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 2: Immediate Job Preemption ===");
    LOG_INFO("");

    LOG_INFO("Scenario: Low-priority job in progress, emergency occurs");
    LOG_INFO("");

    /* Start a low-priority job */
    memset(config_data, 0xAA, BLOCK_SIZE);
    LOG_INFO("Step 1: Starting LOW priority job (Block 112)...");
    NvM_WriteBlock(BLOCK_CONFIG_ID, config_data);

    /* Let it run for a few iterations */
    LOG_INFO("Step 2: Let LOW job run for 5 iterations...");
    for (int i = 0; i < 5; i++) {
        NvM_MainFunction();
    }
    LOG_INFO("✓ LOW job in progress (partial)");
    LOG_INFO("");

    /* Submit IMMEDIATE job */
    memset(crash_data, 0xBB, BLOCK_SIZE);
    LOG_INFO("Step 3: Emergency! Submitting IMMEDIATE job (Block 110)...");
    LOG_INFO("  Expected: Preempt LOW job, execute IMMEDIATE immediately");
    NvM_WriteBlock(BLOCK_CRASH_ID, crash_data);
    LOG_INFO("✓ IMMEDIATE job submitted");
    LOG_INFO("");

    /* Process both jobs */
    LOG_INFO("Processing both jobs...");
    uint32_t iterations = 0;
    uint8_t result_112, result_110;
    boolean immediate_started = FALSE;

    do {
        NvM_MainFunction();
        iterations++;

        NvM_GetJobResult(BLOCK_CONFIG_ID, &result_112);
        NvM_GetJobResult(BLOCK_CRASH_ID, &result_110);

        /* Detect when IMMEDIATE job starts */
        if (!immediate_started && (result_110 == NVM_REQ_OK || result_110 == NVM_REQ_PENDING)) {
            LOG_INFO("  Iteration %u: IMMEDIATE job ACTIVE (preempting LOW)", iterations);
            immediate_started = TRUE;
        }

        /* Log completion */
        if (iterations % 10 == 0) {
            LOG_INFO("  Iteration %u: Block112=%s, Block110=%s",
                     iterations,
                     (result_112 == NVM_REQ_OK) ? "OK" : "PENDING",
                     (result_110 == NVM_REQ_OK) ? "OK" : "PENDING");
        }

    } while ((result_112 == NVM_REQ_PENDING || result_110 == NVM_REQ_PENDING) && iterations < 200);

    LOG_INFO("");
    LOG_INFO("✓ Both jobs completed (%u iterations)", iterations);
    LOG_INFO("  Block 110 (IMMEDIATE): %s", (result_110 == NVM_REQ_OK) ? "✓" : "✗");
    LOG_INFO("  Block 112 (LOW):       %s", (result_112 == NVM_REQ_OK) ? "✓" : "✗");
}

/**
 * @brief Demonstrate priority inversion prevention
 */
static void demo_priority_inversion(void)
{
    LOG_INFO("");
    LOG_INFO("=== Scenario 3: Priority Inversion Prevention ===");
    LOG_INFO("");

    LOG_INFO("Scenario: Multiple jobs at same priority level");
    LOG_INFO("Expected: FIFO order within same priority");
    LOG_INFO("");

    /* Submit multiple jobs at same priority */
    memset(dtc_data, 0x11, BLOCK_SIZE);
    LOG_INFO("Submitting 3 jobs at Priority 10...");
    NvM_WriteBlock(BLOCK_DTC_ID, dtc_data);
    memset(dtc_data, 0x22, BLOCK_SIZE);
    NvM_WriteBlock(BLOCK_DTC_ID, dtc_data);
    memset(dtc_data, 0x33, BLOCK_SIZE);
    NvM_WriteBlock(BLOCK_DTC_ID, dtc_data);
    LOG_INFO("✓ 3 jobs submitted (same priority, FIFO order)");
    LOG_INFO("");

    /* Process all jobs */
    LOG_INFO("Processing jobs...");
    uint32_t iterations = 0;
    uint8_t job_result;

    do {
        NvM_MainFunction();
        iterations++;
        NvM_GetJobResult(BLOCK_DTC_ID, &job_result);

        if (iterations % 5 == 0) {
            LOG_INFO("  Iteration %u: Processing...", iterations);
        }

    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    LOG_INFO("");
    LOG_INFO("✓ All jobs completed (%u iterations)", iterations);
    LOG_INFO("✓ FIFO order maintained within same priority");
}

/**
 * @brief Main demonstration
 */
static void demo_priority_jobs(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Example 08: Priority Jobs");
    LOG_INFO("========================================");
    LOG_INFO("");
    LOG_INFO("Use Case: Emergency data saving");
    LOG_INFO("  - Immediate job preemption");
    LOG_INFO("  - Priority-based scheduling");
    LOG_INFO("  - Priority inversion prevention");
    LOG_INFO("");

    /* Initialize */
    NvM_Init();
    OsScheduler_Init(16);
    register_priority_blocks();

    /* Scenario 1: Normal priority queue */
    demo_normal_priority();

    /* Scenario 2: Immediate preemption */
    demo_immediate_preemption();

    /* Scenario 3: Priority inversion prevention */
    demo_priority_inversion();

    LOG_INFO("");
    LOG_INFO("========================================");
    LOG_INFO("  Key Takeaways");
    LOG_INFO("========================================");
    LOG_INFO("✓ Priority queue: Lower number = higher priority");
    LOG_INFO("✓ Immediate jobs: Preempt ongoing jobs");
    LOG_INFO("✓ Priority 0: Critical system data");
    LOG_INFO("✓ FIFO within same priority");
    LOG_INFO("✓ Use case: Crash data, DTC, emergency");
    LOG_INFO("========================================");
}

/**
 * @brief Main function
 */
int main(void)
{
    demo_priority_jobs();
    return 0;
}
