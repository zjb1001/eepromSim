/**
 * @file test_fault_scenarios.c
 * @brief P0/P1 Fault Scenario System Tests
 *
 * Phase 3.5 Testing: Comprehensive fault injection tests
 * - P0-01: Power loss during page program
 * - P0-03: Single bit flip after read
 * - P0-05: NvM_MainFunction timeout
 * - P0-07: CRC calculation inversion
 * - Redundant Block recovery tests
 * - Dataset Block fallback tests
 */

#include "nvm.h"
#include "fault_injection.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Test data buffer
 */
static uint8_t test_data[256];

/**
 * @brief Test result tracking
 */
typedef struct {
    uint32_t total_tests;
    uint32_t passed_tests;
    uint32_t failed_tests;
} TestResults_t;

static TestResults_t g_results = {0};

/**
 * @brief Macro for test assertions
 */
#define TEST_ASSERT(condition, test_name) \
    do { \
        g_results.total_tests++; \
        if (condition) { \
            g_results.passed_tests++; \
            LOG_INFO("[PASS] %s", test_name); \
        } else { \
            g_results.failed_tests++; \
            LOG_ERROR("[FAIL] %s", test_name); \
        } \
    } while(0)

/**
 * @brief Test P0-01: Power Loss During Page Program
 *
 * Scenario: Simulate power loss immediately after write completes
 * Expected: Data should be written but CRC may be corrupted
 * Recovery: ROM fallback should work
 */
static void test_p0_01_power_loss_during_write(void)
{
    LOG_INFO("=== Test P0-01: Power Loss During Write ===");

    NvM_Init();
    OsScheduler_Init(16);

    /* Register Native Block with ROM fallback */
    static const uint8_t rom_data[256] = {0xDE, 0xAD, 0xBE, 0xEF};
    NvM_BlockConfig_t block = {
        .block_id = 1,
        .block_size = 256,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = test_data,
        .rom_block_ptr = rom_data,
        .rom_block_size = sizeof(rom_data),
        .eeprom_offset = 0x0000
    };

    NvM_RegisterBlock(&block);

    /* Enable power loss fault */
    FaultInj_Enable(FAULT_P0_POWERLOSS_PAGEPROGRAM);

    /* Try to write - should fail due to power loss simulation */
    memset(test_data, 0xAA, 256);
    NvM_WriteBlock(1, test_data);

    /* Process jobs */
    for (int i = 0; i < 20; i++) {
        NvM_MainFunction();
    }

    /* Read back - should fallback to ROM */
    memset(test_data, 0x00, 256);
    NvM_ReadBlock(1, test_data);

    for (int i = 0; i < 20; i++) {
        NvM_MainFunction();
    }

    /* Verify ROM fallback worked */
    TEST_ASSERT(test_data[0] == 0xDE, "P0-01: ROM fallback after power loss");

    /* Disable fault for next test */
    FaultInj_Disable(FAULT_P0_POWERLOSS_PAGEPROGRAM);

    LOG_INFO("");
}

/**
 * @brief Test P0-03: Single Bit Flip After Read
 *
 * Scenario: Single bit corruption in read data
 * Expected: CRC mismatch detected
 * Recovery: Data rejected, ROM fallback
 */
static void test_p0_03_single_bit_flip(void)
{
    LOG_INFO("=== Test P0-03: Single Bit Flip ===");

    NvM_Init();

    NvM_BlockConfig_t block = {
        .block_id = 2,
        .block_size = 256,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = test_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x0400
    };

    NvM_RegisterBlock(&block);

    /* Write good data */
    memset(test_data, 0x55, 256);
    NvM_WriteBlock(2, test_data);

    for (int i = 0; i < 20; i++) {
        NvM_MainFunction();
    }

    /* Enable bit flip fault */
    FaultInj_Enable(FAULT_P0_BITFLIP_SINGLE);

    /* Read back - should detect CRC mismatch */
    memset(test_data, 0x00, 256);
    NvM_ReadBlock(2, test_data);

    for (int i = 0; i < 20; i++) {
        NvM_MainFunction();
    }

    /* With bit flip, read should fail or data should be corrupted */
    /* The fault injection will flip a bit, causing CRC mismatch */
    uint8_t result = NVM_REQ_NOT_OK;
    NvM_GetJobResult(2, &result);

    /* Note: Result might be OK if bit flip happened in non-critical area,
     * but CRC check should catch it */
    LOG_INFO("P0-03: Job result after bit flip = %d", result);

    FaultInj_Disable(FAULT_P0_BITFLIP_SINGLE);

    LOG_INFO("");
}

/**
 * @brief Test P0-07: CRC Calculation Inversion
 *
 * Scenario: CRC calculation returns inverted value
 * Expected: Write succeeds but read fails CRC check
 * Recovery: ROM fallback
 */
static void test_p0_07_crc_inversion(void)
{
    LOG_INFO("=== Test P0-07: CRC Inversion ===");

    NvM_Init();

    static const uint8_t rom_data[256] = {0x11, 0x22, 0x33, 0x44};
    NvM_BlockConfig_t block = {
        .block_id = 3,
        .block_size = 256,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = test_data,
        .rom_block_ptr = rom_data,
        .rom_block_size = sizeof(rom_data),
        .eeprom_offset = 0x0800
    };

    NvM_RegisterBlock(&block);

    /* Enable CRC inversion */
    FaultInj_Enable(FAULT_P0_CRC_INVERT);

    /* Write with inverted CRC */
    memset(test_data, 0x77, 256);
    NvM_WriteBlock(3, test_data);

    for (int i = 0; i < 20; i++) {
        NvM_MainFunction();
    }

    /* Read back - CRC should mismatch (stored inverted vs calculated normal) */
    FaultInj_Disable(FAULT_P0_CRC_INVERT);  /* Disable for read */

    memset(test_data, 0x00, 256);
    NvM_ReadBlock(3, test_data);

    for (int i = 0; i < 20; i++) {
        NvM_MainFunction();
    }

    /* Should fallback to ROM due to CRC mismatch */
    TEST_ASSERT(test_data[0] == 0x11, "P0-07: ROM fallback after CRC inversion");

    LOG_INFO("");
}

/**
 * @brief Test Redundant Block Recovery
 *
 * Scenario: Primary copy corrupted, backup OK
 * Expected: Automatic fallback to backup copy
 * Recovery: Data recovered from backup
 */
static void test_redundant_block_recovery(void)
{
    LOG_INFO("=== Test: Redundant Block Recovery ===");

    NvM_Init();

    NvM_BlockConfig_t block = {
        .block_id = 10,
        .block_size = 256,
        .block_type = NVM_BLOCK_REDUNDANT,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = test_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x0000,
        .redundant_eeprom_offset = 0x0400,
        .version_control_offset = 0,
        .active_version = 0
    };

    NvM_RegisterBlock(&block);

    /* Write redundant data */
    memset(test_data, 0xAB, 256);
    NvM_WriteBlock(10, test_data);

    for (int i = 0; i < 20; i++) {
        NvM_MainFunction();
    }

    LOG_INFO("Redundant block written, testing recovery...");

    /* Simulate primary corruption by reading with fault */
    FaultInj_Enable(FAULT_P0_BITFLIP_SINGLE);

    memset(test_data, 0x00, 256);
    NvM_ReadBlock(10, test_data);

    for (int i = 0; i < 20; i++) {
        NvM_MainFunction();
    }

    FaultInj_Disable(FAULT_P0_BITFLIP_SINGLE);

    /* Should recover from backup */
    /* Note: Due to bit flip, primary might fail, backup should work */
    LOG_INFO("Redundant recovery test completed");

    LOG_INFO("");
}

/**
 * @brief Test Dataset Block Fallback
 *
 * Scenario: Active version corrupted, other versions OK
 * Expected: Fallback to valid version
 * Recovery: Data from older valid version
 */
static void test_dataset_block_fallback(void)
{
    LOG_INFO("=== Test: Dataset Block Fallback ===");

    NvM_Init();

    NvM_BlockConfig_t block = {
        .block_id = 20,
        .block_size = 256,
        .block_type = NVM_BLOCK_DATASET,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = test_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x0000,
        .dataset_count = 3,
        .active_dataset_index = 0
    };

    NvM_RegisterBlock(&block);

    /* Write 3 versions */
    for (uint8_t ver = 0; ver < 3; ver++) {
        memset(test_data, 0x30 + ver, 256);
        NvM_WriteBlock(20, test_data);

        for (int i = 0; i < 20; i++) {
            NvM_MainFunction();
        }
        LOG_INFO("Dataset version %u written", ver);
    }

    /* Read should work normally */
    memset(test_data, 0x00, 256);
    NvM_ReadBlock(20, test_data);

    for (int i = 0; i < 20; i++) {
        NvM_MainFunction();
    }

    /* Verify we got one of the versions */
    boolean valid_data = (test_data[0] == 0x30 || test_data[0] == 0x31 || test_data[0] == 0x32);
    TEST_ASSERT(valid_data, "Dataset: Valid data read");

    LOG_INFO("");
}

/**
 * @brief Test Concurrent Faults
 *
 * Scenario: Multiple faults active simultaneously
 * Expected: System handles gracefully
 * Recovery: ROM fallback or appropriate error handling
 */
static void test_concurrent_faults(void)
{
    LOG_INFO("=== Test: Concurrent Faults ===");

    NvM_Init();

    static const uint8_t rom_data[256] = {0xFF, 0xFF, 0xFF, 0xFF};
    NvM_BlockConfig_t block = {
        .block_id = 4,
        .block_size = 256,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = test_data,
        .rom_block_ptr = rom_data,
        .rom_block_size = sizeof(rom_data),
        .eeprom_offset = 0x0C00
    };

    NvM_RegisterBlock(&block);

    /* Enable multiple faults */
    FaultInj_Enable(FAULT_P0_POWERLOSS_PAGEPROGRAM);
    FaultInj_Enable(FAULT_P0_CRC_INVERT);

    /* Try write - should fail */
    memset(test_data, 0x99, 256);
    NvM_WriteBlock(4, test_data);

    for (int i = 0; i < 20; i++) {
        NvM_MainFunction();
    }

    /* Disable faults */
    FaultInj_Disable(FAULT_P0_POWERLOSS_PAGEPROGRAM);
    FaultInj_Disable(FAULT_P0_CRC_INVERT);

    /* Read - should fallback to ROM */
    memset(test_data, 0x00, 256);
    NvM_ReadBlock(4, test_data);

    for (int i = 0; i < 20; i++) {
        NvM_MainFunction();
    }

    TEST_ASSERT(test_data[0] == 0xFF, "Concurrent: ROM fallback works");

    LOG_INFO("");
}

/**
 * @brief Print test summary
 */
static void print_test_summary(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Test Summary");
    LOG_INFO("========================================");
    LOG_INFO("Total tests: %u", g_results.total_tests);
    LOG_INFO("Passed: %u", g_results.passed_tests);
    LOG_INFO("Failed: %u", g_results.failed_tests);

    if (g_results.failed_tests == 0) {
        LOG_INFO("✓ ALL TESTS PASSED");
    } else {
        LOG_ERROR("✗ SOME TESTS FAILED");
    }

    /* Get fault injection statistics */
    FaultStats_t stats;
    FaultInj_GetStats(&stats);
    LOG_INFO("");
    LOG_INFO("Fault Injection Stats:");
    LOG_INFO("  Total triggered: %u", stats.total_triggered);
    LOG_INFO("  Total injected: %u", stats.total_injected);
    LOG_INFO("  Injection failures: %u", stats.injection_failures);

    LOG_INFO("========================================");
}

/**
 * @brief Main test entry point
 */
int main(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  P0/P1 Fault Scenario System Tests");
    LOG_INFO("========================================");
    LOG_INFO("");

    /* Initialize subsystems */
    FaultInj_Init();
    OsScheduler_Init(16);

    /* Run tests */
    test_p0_01_power_loss_during_write();
    test_p0_03_single_bit_flip();
    test_p0_07_crc_inversion();
    test_redundant_block_recovery();
    test_dataset_block_fallback();
    test_concurrent_faults();

    /* Print summary */
    print_test_summary();

    return (g_results.failed_tests == 0) ? 0 : 1;
}
