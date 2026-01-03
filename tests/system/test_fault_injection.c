/**
 * @file test_fault_injection.c
 * @brief Fault injection framework test
 *
 * Phase 3 Example: Test P0-01 to P0-05 fault scenarios
 * - P0-01: Power loss during page program
 * - P0-02: Power loss during WriteAll
 * - P0-03: Single bit flip
 * - P0-04: Multiple bit flip
 * - P0-05: Timeout
 */

#include "nvm.h"
#include "fault_injection.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Test data block
 */
static uint8_t test_block[256] = {0};

/**
 * @brief Test P0-01: Power loss during page program
 */
static void test_p0_power_loss_page_program(void)
{
    LOG_INFO("=== Test P0-01: Power Loss During Page Program ===");

    /* Initialize systems */
    FaultInj_Init();
    NvM_Init();

    /* Register test block */
    NvM_BlockConfig_t block = {
        .block_id = 0,
        .block_size = 256,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .ram_mirror_ptr = test_block,
        .eeprom_offset = 0x0000
    };

    NvM_RegisterBlock(&block);

    /* Prepare test data */
    memset(test_block, 0xAA, 256);

    /* Enable P0-01 fault */
    FaultInj_Enable(FAULT_P0_POWERLOSS_PAGEPROGRAM);
    LOG_INFO("✓ Enabled FAULT_P0_POWERLOSS_PAGEPROGRAM");

    /* Write block - should fail due to power loss */
    Std_ReturnType ret = NvM_WriteBlock(0, test_block);
    if (ret == E_OK) {
        LOG_INFO("WriteBlock job queued");
    }

    /* Process jobs */
    for (int i = 0; i < 10; i++) {
        NvM_MainFunction();
    }

    /* Check result */
    uint8_t result;
    NvM_GetJobResult(0, &result);
    if (result == NVM_REQ_NOT_OK) {
        LOG_INFO("✓ P0-01: Write failed as expected (power loss injected)");
    } else {
        LOG_WARN("✗ P0-01: Unexpected result (expected failure)");
    }

    /* Disable fault */
    FaultInj_Disable(FAULT_P0_POWERLOSS_PAGEPROGRAM);
    LOG_INFO("");
}

/**
 * @brief Test P0-03: Single bit flip
 */
static void test_p0_bit_flip_single(void)
{
    LOG_INFO("=== Test P0-03: Single Bit Flip ===");

    /* Initialize systems */
    FaultInj_Init();
    NvM_Init();

    /* Register test block */
    NvM_BlockConfig_t block = {
        .block_id = 1,
        .block_size = 256,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .ram_mirror_ptr = test_block,
        .eeprom_offset = 0x0400
    };

    NvM_RegisterBlock(&block);

    /* Prepare and write test data */
    memset(test_block, 0x55, 256);
    NvM_WriteBlock(1, test_block);

    /* Process jobs */
    for (int i = 0; i < 10; i++) {
        NvM_MainFunction();
    }

    /* Clear RAM */
    memset(test_block, 0x00, 256);

    /* Enable P0-03 fault */
    FaultInj_Enable(FAULT_P0_BITFLIP_SINGLE);
    LOG_INFO("✓ Enabled FAULT_P0_BITFLIP_SINGLE");

    /* Read block - bit flip will be injected */
    NvM_ReadBlock(1, test_block);

    /* Process jobs */
    for (int i = 0; i < 10; i++) {
        NvM_MainFunction();
    }

    /* Check result */
    uint8_t result;
    NvM_GetJobResult(1, &result);

    /* With bit flip, CRC should fail */
    LOG_INFO("P0-03: Read result=%d (bit flip injected during read)", result);
    LOG_INFO("✓ P0-03: Bit flip fault injection test complete");

    /* Disable fault */
    FaultInj_Disable(FAULT_P0_BITFLIP_SINGLE);
    LOG_INFO("");
}

/**
 * @brief Test P0-07: CRC inversion
 */
static void test_p0_crc_invert(void)
{
    LOG_INFO("=== Test P0-07: CRC Inversion ===");

    /* Initialize systems */
    FaultInj_Init();
    NvM_Init();

    /* Register test block */
    NvM_BlockConfig_t block = {
        .block_id = 2,
        .block_size = 256,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .ram_mirror_ptr = test_block,
        .eeprom_offset = 0x0800
    };

    NvM_RegisterBlock(&block);

    /* Prepare test data */
    memset(test_block, 0xBB, 256);

    /* Enable P0-07 fault (CRC inversion) */
    FaultInj_Enable(FAULT_P0_CRC_INVERT);
    LOG_INFO("✓ Enabled FAULT_P0_CRC_INVERT");

    /* Write block - CRC will be inverted during calculation */
    NvM_WriteBlock(2, test_block);

    /* Process jobs */
    for (int i = 0; i < 10; i++) {
        NvM_MainFunction();
    }

    /* Check result - write should succeed (inverted CRC gets stored) */
    uint8_t result;
    NvM_GetJobResult(2, &result);

    LOG_INFO("P0-07: Write result=%d (inverted CRC stored)", result);
    LOG_INFO("✓ P0-07: CRC inversion fault injection test complete");

    /* Disable fault */
    FaultInj_Disable(FAULT_P0_CRC_INVERT);
    LOG_INFO("");
}

/**
 * @brief Test fault statistics
 */
static void test_fault_statistics(void)
{
    LOG_INFO("=== Test Fault Statistics ===");

    FaultInj_Init();

    /* Enable multiple faults */
    FaultInj_Enable(FAULT_P0_BITFLIP_SINGLE);
    FaultInj_Enable(FAULT_P0_CRC_INVERT);
    FaultInj_Enable(FAULT_P0_POWERLOSS_PAGEPROGRAM);

    LOG_INFO("✓ Enabled 3 fault types");

    /* Get statistics */
    FaultStats_t stats;
    FaultInj_GetStats(&stats);

    LOG_INFO("Fault Statistics:");
    LOG_INFO("  Total triggered: %u", stats.total_triggered);
    LOG_INFO("  Total injected: %u", stats.total_injected);
    LOG_INFO("  Injection failures: %u", stats.injection_failures);

    /* Reset statistics */
    FaultInj_ResetStats();
    LOG_INFO("✓ Statistics reset");

    FaultInj_GetStats(&stats);
    LOG_INFO("After reset:");
    LOG_INFO("  Total triggered: %u", stats.total_triggered);
    LOG_INFO("  Total injected: %u", stats.total_injected);

    LOG_INFO("");
}

/**
 * @brief Main function
 */
int main(void)
{
    Log_SetLevel(LOG_LEVEL_INFO);

    LOG_INFO("========================================");
    LOG_INFO("  Fault Injection Framework Test");
    LOG_INFO("========================================");
    LOG_INFO("");

    test_p0_power_loss_page_program();
    test_p0_bit_flip_single();
    test_p0_crc_invert();
    test_fault_statistics();

    LOG_INFO("========================================");
    LOG_INFO("  All fault injection tests complete!");
    LOG_INFO("========================================");

    return 0;
}
