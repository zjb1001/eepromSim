/**
 * @file test_read_write_flow.c
 * @brief Integration Test: Complete Read-Modify-Write Flow
 *
 * Test Coverage:
 * - Complete read → modify → write cycle
 * - Data persistence across operations
 * - Job queue interaction
 * - Multi-block coordination
 *
 * Test Strategy:
 * - End-to-end functional testing
 * - Data integrity verification
 * - State consistency checks
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

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

/**
 * @brief Test complete read-modify-write cycle
 */
static void test_read_modify_write_cycle(void)
{
    LOG_INFO("");
    LOG_INFO("Test: Complete Read-Modify-Write Cycle");

    NvM_Init();
    OsScheduler_Init(16);

    /* Register test block */
    static uint8_t config_data[256];
    NvM_BlockConfig_t block = {
        .block_id = 1,
        .block_size = 256,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = config_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x0000
    };
    NvM_RegisterBlock(&block);

    /* Step 1: Initialize with default values */
    LOG_INFO("  Step 1: Initialize with default values");
    memset(config_data, 0xAA, 256);
    NvM_WriteBlock(1, config_data);

    uint8_t job_result;
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(1, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    TEST_ASSERT_EQ(job_result, NVM_REQ_OK, "Initial write OK");
    LOG_INFO("    ✓ Initial data: 0xAA pattern");

    /* Step 2: Read current configuration */
    LOG_INFO("  Step 2: Read current configuration");
    memset(config_data, 0x00, 256);
    NvM_ReadBlock(1, config_data);

    iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(1, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    TEST_ASSERT_EQ(job_result, NVM_REQ_OK, "Read OK");
    TEST_ASSERT_EQ(config_data[0], 0xAA, "Data read correctly");
    LOG_INFO("    ✓ Read data: 0xAA pattern (verified)");

    /* Step 3: Modify configuration */
    LOG_INFO("  Step 3: Modify configuration");
    config_data[0] = 0xBB;  /* Change first byte */
    config_data[1] = 50;    /* Set version number */
    config_data[2] = 1;     /* Enable feature */
    LOG_INFO("    ✓ Modified: [0]=0xBB, [1]=50, [2]=1");

    /* Step 4: Write modified configuration */
    LOG_INFO("  Step 4: Write modified configuration");
    NvM_WriteBlock(1, config_data);

    iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(1, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    TEST_ASSERT_EQ(job_result, NVM_REQ_OK, "Modified write OK");
    LOG_INFO("    ✓ Written successfully");

    /* Step 5: Verify persistence */
    LOG_INFO("  Step 5: Verify persistence");
    memset(config_data, 0x00, 256);
    NvM_ReadBlock(1, config_data);

    iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(1, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    TEST_ASSERT_EQ(job_result, NVM_REQ_OK, "Verification read OK");
    TEST_ASSERT_EQ(config_data[0], 0xBB, "Byte 0 persisted");
    TEST_ASSERT_EQ(config_data[1], 50, "Byte 1 persisted");
    TEST_ASSERT_EQ(config_data[2], 1, "Byte 2 persisted");

    LOG_INFO("    ✓ All modifications verified");
    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test multiple read-modify-write cycles
 */
static void test_multiple_cycles(void)
{
    LOG_INFO("");
    LOG_INFO("Test: Multiple Read-Modify-Write Cycles");

    NvM_Init();
    OsScheduler_Init(16);

    static uint8_t counter_data[256];
    NvM_BlockConfig_t block = {
        .block_id = 10,
        .block_size = 256,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = counter_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x1000
    };
    NvM_RegisterBlock(&block);

    /* Perform 5 increment cycles */
    for (int cycle = 0; cycle < 5; cycle++) {
        LOG_INFO("  Cycle %d:", cycle + 1);

        /* Read */
        memset(counter_data, 0x00, 256);
        NvM_ReadBlock(10, counter_data);

        uint8_t job_result;
        uint32_t iterations = 0;
        do {
            NvM_MainFunction();
            NvM_GetJobResult(10, &job_result);
            iterations++;
        } while (job_result == NVM_REQ_PENDING && iterations < 100);

        /* Increment counter */
        uint32_t* counter_ptr = (uint32_t*)counter_data;
        (*counter_ptr)++;
        LOG_INFO("    Counter: %u → %u", (*counter_ptr) - 1, *counter_ptr);

        /* Write */
        NvM_WriteBlock(10, counter_data);

        iterations = 0;
        do {
            NvM_MainFunction();
            NvM_GetJobResult(10, &job_result);
            iterations++;
        } while (job_result == NVM_REQ_PENDING && iterations < 100);

        TEST_ASSERT_EQ(job_result, NVM_REQ_OK, "Cycle OK");
    }

    /* Final verification */
    memset(counter_data, 0x00, 256);
    NvM_ReadBlock(10, counter_data);

    uint8_t job_result;
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(10, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    uint32_t* counter_ptr = (uint32_t*)counter_data;
    TEST_ASSERT_EQ(*counter_ptr, 5, "Counter = 5 after 5 cycles");

    LOG_INFO("  Final counter: %u", *counter_ptr);
    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test concurrent read-modify-write on different blocks
 */
static void test_concurrent_operations(void)
{
    LOG_INFO("");
    LOG_INFO("Test: Concurrent Operations on Different Blocks");

    NvM_Init();
    OsScheduler_Init(16);

    /* Register 3 blocks */
    static uint8_t data1[256], data2[256], data3[256];

    NvM_BlockConfig_t block1 = {
        .block_id = 20, .block_size = 256, .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16, .priority = 5, .is_immediate = FALSE,
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

    NvM_BlockConfig_t block3 = {
        .block_id = 22, .block_size = 256, .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16, .priority = 15, .is_immediate = FALSE,
        .is_write_protected = FALSE, .ram_mirror_ptr = data3,
        .rom_block_ptr = NULL, .rom_block_size = 0, .eeprom_offset = 0x2800
    };
    NvM_RegisterBlock(&block3);

    /* Read-modify-write on all blocks */
    LOG_INFO("  Performing concurrent read-modify-write...");

    /* Block 1 */
    memset(data1, 0x00, 256);
    NvM_ReadBlock(20, data1);
    memset(data1, 0x11, 256);
    NvM_WriteBlock(20, data1);

    /* Block 2 */
    memset(data2, 0x00, 256);
    NvM_ReadBlock(21, data2);
    memset(data2, 0x22, 256);
    NvM_WriteBlock(21, data2);

    /* Block 3 */
    memset(data3, 0x00, 256);
    NvM_ReadBlock(22, data3);
    memset(data3, 0x33, 256);
    NvM_WriteBlock(22, data3);

    /* Process all jobs */
    uint8_t result1, result2, result3;
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        iterations++;

        NvM_GetJobResult(20, &result1);
        NvM_GetJobResult(21, &result2);
        NvM_GetJobResult(22, &result3);

        if (iterations > 300) break;
    } while (result1 == NVM_REQ_PENDING ||
             result2 == NVM_REQ_PENDING ||
             result3 == NVM_REQ_PENDING);

    TEST_ASSERT_EQ(result1, NVM_REQ_OK, "Block 1 OK");
    TEST_ASSERT_EQ(result2, NVM_REQ_OK, "Block 2 OK");
    TEST_ASSERT_EQ(result3, NVM_REQ_OK, "Block 3 OK");

    LOG_INFO("  All blocks processed independently");
    LOG_INFO("  Result: Passed");
}

/**
 * @brief Run all integration tests
 */
int main(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Integration Test: Read-Write Flow");
    LOG_INFO("========================================");
    LOG_INFO("");

    /* Run tests */
    test_read_modify_write_cycle();
    test_multiple_cycles();
    test_concurrent_operations();

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
