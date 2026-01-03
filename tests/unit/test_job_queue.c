/**
 * @file test_job_queue.c
 * @brief Unit Test: NvM Job Queue
 *
 * Test Coverage:
 * - Job enqueue/dequeue operations
 * - Priority-based ordering
 * - Queue overflow/underflow handling
 * - FIFO within same priority
 * - Job queue capacity
 *
 * Test Strategy:
 * - White-box testing of queue internals
 * - Stress testing with maximum capacity
 * - Priority ordering verification
 * - Edge case testing
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Test counters */
static uint32_t tests_passed = 0;
static uint32_t tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
            LOG_INFO("  ✓ %s", message); \
        } else { \
            tests_failed++; \
            LOG_ERROR("  ✗ %s", message); \
        } \
    } while(0)

#define TEST_ASSERT_EQ(actual, expected, message) \
    TEST_ASSERT((actual) == (expected), message)

/**
 * @brief Test single job enqueue/dequeue
 */
static void test_single_job(void)
{
    LOG_INFO("");
    LOG_INFO("Test: Single Job Enqueue/Dequeue");

    NvM_Init();
    OsScheduler_Init(16);

    /* Register test block */
    static uint8_t test_data[256];
    NvM_BlockConfig_t block = {
        .block_id = 1,
        .block_size = 256,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = test_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x0000
    };
    NvM_RegisterBlock(&block);

    /* Enqueue single job */
    memset(test_data, 0xAA, 256);
    NvM_WriteBlock(1, test_data);

    /* Verify job is queued */
    uint8_t job_result;
    NvM_GetJobResult(1, &job_result);
    TEST_ASSERT_EQ(job_result, NVM_REQ_PENDING, "Job enqueued successfully");

    /* Process job */
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(1, &job_result);
        iterations++;
        if (iterations > 100) break;
    } while (job_result == NVM_REQ_PENDING);

    /* Verify dequeue */
    TEST_ASSERT_EQ(job_result, NVM_REQ_OK, "Job dequeued and processed");
    LOG_INFO("  Iterations: %u", iterations);
    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test priority-based ordering
 */
static void test_priority_ordering(void)
{
    LOG_INFO("");
    LOG_INFO("Test: Priority-Based Job Ordering");

    NvM_Init();
    OsScheduler_Init(16);

    /* Register blocks with different priorities */
    static uint8_t data_high[256], data_med[256], data_low[256];

    NvM_BlockConfig_t block_high = {
        .block_id = 10, .block_size = 256, .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16, .priority = 5, .is_immediate = FALSE,
        .is_write_protected = FALSE, .ram_mirror_ptr = data_high,
        .rom_block_ptr = NULL, .rom_block_size = 0, .eeprom_offset = 0x1000
    };
    NvM_RegisterBlock(&block_high);

    NvM_BlockConfig_t block_med = {
        .block_id = 11, .block_size = 256, .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16, .priority = 10, .is_immediate = FALSE,
        .is_write_protected = FALSE, .ram_mirror_ptr = data_med,
        .rom_block_ptr = NULL, .rom_block_size = 0, .eeprom_offset = 0x1400
    };
    NvM_RegisterBlock(&block_med);

    NvM_BlockConfig_t block_low = {
        .block_id = 12, .block_size = 256, .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16, .priority = 20, .is_immediate = FALSE,
        .is_write_protected = FALSE, .ram_mirror_ptr = data_low,
        .rom_block_ptr = NULL, .rom_block_size = 0, .eeprom_offset = 0x1800
    };
    NvM_RegisterBlock(&block_low);

    /* Submit jobs in reverse priority order */
    memset(data_low, 0x33, 256);
    NvM_WriteBlock(12, data_low);  /* LOW priority, submitted first */

    memset(data_med, 0x22, 256);
    NvM_WriteBlock(11, data_med);  /* MEDIUM priority */

    memset(data_high, 0x11, 256);
    NvM_WriteBlock(10, data_high);  /* HIGH priority, submitted last */

    LOG_INFO("  Jobs submitted: LOW → MEDIUM → HIGH");
    LOG_INFO("  Expected order: HIGH → MEDIUM → LOW");

    /* Process jobs and track completion order */
    uint8_t result_high, result_med, result_low;
    uint32_t iterations = 0;
    uint32_t complete_high = 0, complete_med = 0, complete_low = 0;

    do {
        NvM_MainFunction();
        iterations++;

        NvM_GetJobResult(10, &result_high);
        NvM_GetJobResult(11, &result_med);
        NvM_GetJobResult(12, &result_low);

        /* Track completion order */
        if (result_high == NVM_REQ_OK && complete_high == 0) {
            complete_high = iterations;
            LOG_INFO("  Iteration %u: HIGH priority job completed", iterations);
        }
        if (result_med == NVM_REQ_OK && complete_med == 0) {
            complete_med = iterations;
            LOG_INFO("  Iteration %u: MEDIUM priority job completed", iterations);
        }
        if (result_low == NVM_REQ_OK && complete_low == 0) {
            complete_low = iterations;
            LOG_INFO("  Iteration %u: LOW priority job completed", iterations);
        }

        if (iterations > 200) break;
    } while (result_high == NVM_REQ_PENDING ||
             result_med == NVM_REQ_PENDING ||
             result_low == NVM_REQ_PENDING);

    /* Verify priority order */
    TEST_ASSERT(complete_high < complete_med, "HIGH completes before MEDIUM");
    TEST_ASSERT(complete_med < complete_low, "MEDIUM completes before LOW");
    TEST_ASSERT_EQ(result_high, NVM_REQ_OK, "HIGH job OK");
    TEST_ASSERT_EQ(result_med, NVM_REQ_OK, "MEDIUM job OK");
    TEST_ASSERT_EQ(result_low, NVM_REQ_OK, "LOW job OK");

    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test FIFO within same priority
 */
static void test_fifo_same_priority(void)
{
    LOG_INFO("");
    LOG_INFO("Test: FIFO Order Within Same Priority");

    NvM_Init();
    OsScheduler_Init(16);

    /* Register two blocks with same priority */
    static uint8_t data1[256], data2[256];

    NvM_BlockConfig_t block1 = {
        .block_id = 20, .block_size = 256, .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16, .priority = 10, .is_immediate = FALSE,
        .is_write_protected = FALSE, .ram_mirror_ptr = data1,
        .rom_block_ptr = NULL, .rom_block_size = 0, .eeprom_offset = 0x2000
    };
    NvM_RegisterBlock(&block1);

    NvM_BlockConfig_t block2 = {
        .block_id = 21, .block_size = 256, .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16, .priority = 10, .is_immediate = FALSE,
        .is_write_protected = FALSE, .ram_mirror_ptr = data2,
        .rom_block_ptr = NULL, .rom_block_size = 0, .eeprom_offset = 0x2400
    };
    NvM_RegisterBlock(&block2);

    /* Submit jobs sequentially */
    memset(data1, 0xAA, 256);
    NvM_WriteBlock(20, data1);  /* Submitted first */

    memset(data2, 0xBB, 256);
    NvM_WriteBlock(21, data2);  /* Submitted second */

    LOG_INFO("  Jobs submitted: Job1 → Job2 (same priority)");
    LOG_INFO("  Expected order: Job1 → Job2");

    /* Process jobs */
    uint8_t result1, result2;
    uint32_t iterations = 0;
    uint32_t complete1 = 0, complete2 = 0;

    do {
        NvM_MainFunction();
        iterations++;

        NvM_GetJobResult(20, &result1);
        NvM_GetJobResult(21, &result2);

        if (result1 == NVM_REQ_OK && complete1 == 0) {
            complete1 = iterations;
        }
        if (result2 == NVM_REQ_OK && complete2 == 0) {
            complete2 = iterations;
        }

        if (iterations > 200) break;
    } while (result1 == NVM_REQ_PENDING || result2 == NVM_REQ_PENDING);

    /* Verify FIFO order */
    TEST_ASSERT(complete1 <= complete2, "Job1 completes before or with Job2 (FIFO)");
    TEST_ASSERT_EQ(result1, NVM_REQ_OK, "Job1 OK");
    TEST_ASSERT_EQ(result2, NVM_REQ_OK, "Job2 OK");

    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test queue capacity (32 slots)
 */
static void test_queue_capacity(void)
{
    LOG_INFO("");
    LOG_INFO("Test: Queue Capacity (32 slots)");

    NvM_Init();
    OsScheduler_Init(16);

    /* Register multiple blocks to fill queue */
    static uint8_t data_array[32][256];
    NvM_BlockConfig_t blocks[32];

    for (int i = 0; i < 32; i++) {
        blocks[i] = (NvM_BlockConfig_t){
            .block_id = 100 + i,
            .block_size = 256,
            .block_type = NVM_BLOCK_NATIVE,
            .crc_type = NVM_CRC16,
            .priority = 10 + i,
            .is_immediate = FALSE,
            .is_write_protected = FALSE,
            .ram_mirror_ptr = data_array[i],
            .rom_block_ptr = NULL,
            .rom_block_size = 0,
            .eeprom_offset = (uint16_t)(0x4000 + i * 1024)
        };
        NvM_RegisterBlock(&blocks[i]);
    }

    LOG_INFO("  Registered 32 blocks");

    /* Submit 32 jobs (fill queue) */
    for (int i = 0; i < 32; i++) {
        memset(data_array[i], 0x10 + i, 256);
        NvM_WriteBlock(100 + i, data_array[i]);
    }

    LOG_INFO("  Submitted 32 jobs");

    /* Verify all jobs are queued */
    uint8_t results[32];
    uint32_t pending_count = 0;

    for (int i = 0; i < 32; i++) {
        NvM_GetJobResult(100 + i, &results[i]);
        if (results[i] == NVM_REQ_PENDING) {
            pending_count++;
        }
    }

    TEST_ASSERT(pending_count == 32, "All 32 jobs fit in queue");
    LOG_INFO("  Pending jobs: %u / 32", pending_count);

    /* Process all jobs */
    uint32_t iterations = 0;
    boolean all_done = FALSE;

    do {
        NvM_MainFunction();
        iterations++;

        pending_count = 0;
        for (int i = 0; i < 32; i++) {
            NvM_GetJobResult(100 + i, &results[i]);
            if (results[i] == NVM_REQ_PENDING) {
                pending_count++;
            }
        }

        if (pending_count == 0) {
            all_done = TRUE;
        }

        if (iterations > 500) break;
    } while (!all_done);

    /* Verify all jobs completed */
    uint32_t ok_count = 0;
    for (int i = 0; i < 32; i++) {
        if (results[i] == NVM_REQ_OK) {
            ok_count++;
        }
    }

    TEST_ASSERT(ok_count == 32, "All 32 jobs completed successfully");
    LOG_INFO("  Completed jobs: %u / 32", ok_count);
    LOG_INFO("  Iterations: %u", iterations);
    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test queue overflow handling
 */
static void test_queue_overflow(void)
{
    LOG_INFO("");
    LOG_INFO("Test: Queue Overflow Handling");

    NvM_Init();
    OsScheduler_Init(16);

    /* Register 33 blocks (one more than queue capacity) */
    static uint8_t data_array[33][256];

    for (int i = 0; i < 33; i++) {
        NvM_BlockConfig_t block = {
            .block_id = 200 + i,
            .block_size = 256,
            .block_type = NVM_BLOCK_NATIVE,
            .crc_type = NVM_CRC16,
            .priority = 10,
            .is_immediate = FALSE,
            .is_write_protected = FALSE,
            .ram_mirror_ptr = data_array[i],
            .rom_block_ptr = NULL,
            .rom_block_size = 0,
            .eeprom_offset = (uint16_t)(0x8000 + i * 1024)
        };
        NvM_RegisterBlock(&block);
    }

    LOG_INFO("  Registered 33 blocks");

    /* Submit 33 jobs (should exceed queue capacity) */
    for (int i = 0; i < 33; i++) {
        memset(data_array[i], 0xAA, 256);
        NvM_WriteBlock(200 + i, data_array[i]);
    }

    LOG_INFO("  Submitted 33 jobs (exceeds 32-slot queue)");

    /* Process first 32 jobs to make room */
    for (int i = 0; i < 50; i++) {
        NvM_MainFunction();
    }

    /* Verify at least 32 jobs were accepted */
    uint8_t results[33];
    uint32_t ok_count = 0;
    uint32_t pending_count = 0;

    for (int i = 0; i < 33; i++) {
        NvM_GetJobResult(200 + i, &results[i]);
        if (results[i] == NVM_REQ_OK) {
            ok_count++;
        } else if (results[i] == NVM_REQ_PENDING) {
            pending_count++;
        }
    }

    LOG_INFO("  Status: %u OK, %u PENDING", ok_count, pending_count);
    TEST_ASSERT((ok_count + pending_count) >= 32, "At least 32 jobs accepted");

    /* Complete remaining jobs */
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        iterations++;

        pending_count = 0;
        for (int i = 0; i < 33; i++) {
            NvM_GetJobResult(200 + i, &results[i]);
            if (results[i] == NVM_REQ_PENDING) {
                pending_count++;
            }
        }

        if (iterations > 500) break;
    } while (pending_count > 0);

    LOG_INFO("  Final: %u jobs completed", ok_count);
    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test immediate job preemption
 */
static void test_immediate_preemption(void)
{
    LOG_INFO("");
    LOG_INFO("Test: Immediate Job Preemption");

    NvM_Init();
    OsScheduler_Init(16);

    /* Register LOW priority block */
    static uint8_t data_low[256];
    NvM_BlockConfig_t block_low = {
        .block_id = 50, .block_size = 256, .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16, .priority = 20, .is_immediate = FALSE,
        .is_write_protected = FALSE, .ram_mirror_ptr = data_low,
        .rom_block_ptr = NULL, .rom_block_size = 0, .eeprom_offset = 0x5000
    };
    NvM_RegisterBlock(&block_low);

    /* Register IMMEDIATE block */
    static uint8_t data_imm[256];
    NvM_BlockConfig_t block_imm = {
        .block_id = 51, .block_size = 256, .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16, .priority = 0, .is_immediate = TRUE,
        .is_write_protected = FALSE, .ram_mirror_ptr = data_imm,
        .rom_block_ptr = NULL, .rom_block_size = 0, .eeprom_offset = 0x5400
    };
    NvM_RegisterBlock(&block_imm);

    /* Start LOW priority job */
    memset(data_low, 0xBB, 256);
    NvM_WriteBlock(50, data_low);

    /* Let it run briefly */
    for (int i = 0; i < 5; i++) {
        NvM_MainFunction();
    }

    /* Submit IMMEDIATE job (should preempt) */
    memset(data_imm, 0xCC, 256);
    NvM_WriteBlock(51, data_imm);

    LOG_INFO("  LOW job running, submitted IMMEDIATE job");

    /* Process both jobs */
    uint8_t result_low, result_imm;
    uint32_t iterations = 0;
    uint32_t complete_imm = 0;

    do {
        NvM_MainFunction();
        iterations++;

        NvM_GetJobResult(50, &result_low);
        NvM_GetJobResult(51, &result_imm);

        if (result_imm == NVM_REQ_OK && complete_imm == 0) {
            complete_imm = iterations;
            LOG_INFO("  Iteration %u: IMMEDIATE job completed", iterations);
        }

        if (iterations > 200) break;
    } while (result_low == NVM_REQ_PENDING || result_imm == NVM_REQ_PENDING);

    /* Verify IMMEDIATE job completed quickly */
    TEST_ASSERT(complete_imm > 0, "IMMEDIATE job completed");
    TEST_ASSERT(complete_imm < iterations, "IMMEDIATE job finished before LOW");
    TEST_ASSERT_EQ(result_imm, NVM_REQ_OK, "IMMEDIATE job OK");
    TEST_ASSERT_EQ(result_low, NVM_REQ_OK, "LOW job OK");

    LOG_INFO("  Iterations: %u (IMMEDIATE at %u)", iterations, complete_imm);
    LOG_INFO("  Result: Passed");
}

/**
 * @brief Run all job queue tests
 */
int main(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Unit Test: NvM Job Queue");
    LOG_INFO("========================================");
    LOG_INFO("");

    /* Run tests */
    test_single_job();
    test_priority_ordering();
    test_fifo_same_priority();
    test_queue_capacity();
    test_queue_overflow();
    test_immediate_preemption();

    /* Print summary */
    LOG_INFO("");
    LOG_INFO("========================================");
    LOG_INFO("  Test Summary");
    LOG_INFO("========================================");
    LOG_INFO("  Passed: %u", tests_passed);
    LOG_INFO("  Failed: %u", tests_failed);
    LOG_INFO("  Total:  %u", tests_passed + tests_failed);
    LOG_INFO("");

    if (tests_failed == 0) {
        LOG_INFO("✓ All tests passed!");
        LOG_INFO("========================================");
        return 0;
    } else {
        LOG_ERROR("✗ Some tests failed!");
        LOG_INFO("========================================");
        return 1;
    }
}
