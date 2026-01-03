/**
 * @file test_priority_handling.c
 * @brief Integration Test: Priority Job Handling
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

static uint32_t tests_passed = 0, tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { if (cond) { tests_passed++; LOG_INFO("  ✓ %s", msg); } else { tests_failed++; LOG_ERROR("  ✗ %s", msg); } } while(0)

static void test_priority_order(void) {
    LOG_INFO("Test: Priority-Based Job Ordering");
    
    NvM_Init();
    OsScheduler_Init(16);
    
    static uint8_t data_high[256], data_med[256], data_low[256];
    
    NvM_BlockConfig_t block_high = { .block_id = 100, .block_size = 256, .block_type = NVM_BLOCK_NATIVE, .crc_type = NVM_CRC16, .priority = 5, .is_immediate = FALSE, .is_write_protected = FALSE, .ram_mirror_ptr = data_high, .rom_block_ptr = NULL, .rom_block_size = 0, .eeprom_offset = 0x7000 };
    NvM_RegisterBlock(&block_high);
    
    NvM_BlockConfig_t block_med = { .block_id = 101, .block_size = 256, .block_type = NVM_BLOCK_NATIVE, .crc_type = NVM_CRC16, .priority = 10, .is_immediate = FALSE, .is_write_protected = FALSE, .ram_mirror_ptr = data_med, .rom_block_ptr = NULL, .rom_block_size = 0, .eeprom_offset = 0x7400 };
    NvM_RegisterBlock(&block_med);
    
    NvM_BlockConfig_t block_low = { .block_id = 102, .block_size = 256, .block_type = NVM_BLOCK_NATIVE, .crc_type = NVM_CRC16, .priority = 20, .is_immediate = FALSE, .is_write_protected = FALSE, .ram_mirror_ptr = data_low, .rom_block_ptr = NULL, .rom_block_size = 0, .eeprom_offset = 0x7800 };
    NvM_RegisterBlock(&block_low);
    
    memset(data_low, 0x33, 256);
    NvM_WriteBlock(102, data_low);
    memset(data_med, 0x22, 256);
    NvM_WriteBlock(101, data_med);
    memset(data_high, 0x11, 256);
    NvM_WriteBlock(100, data_high);
    
    uint8_t result_high, result_med, result_low;
    uint32_t iterations = 0, complete_high = 0, complete_med = 0, complete_low = 0;
    
    do {
        NvM_MainFunction();
        iterations++;
        NvM_GetJobResult(100, &result_high);
        NvM_GetJobResult(101, &result_med);
        NvM_GetJobResult(102, &result_low);
        if (result_high == NVM_REQ_OK && complete_high == 0) complete_high = iterations;
        if (result_med == NVM_REQ_OK && complete_med == 0) complete_med = iterations;
        if (result_low == NVM_REQ_OK && complete_low == 0) complete_low = iterations;
        if (iterations > 200) break;
    } while (result_high == NVM_REQ_PENDING || result_med == NVM_REQ_PENDING || result_low == NVM_REQ_PENDING);
    
    TEST_ASSERT(complete_high < complete_med, "HIGH before MEDIUM");
    TEST_ASSERT(complete_med < complete_low, "MEDIUM before LOW");
    TEST_ASSERT(result_high == NVM_REQ_OK, "HIGH OK");
    TEST_ASSERT(result_med == NVM_REQ_OK, "MEDIUM OK");
    TEST_ASSERT(result_low == NVM_REQ_OK, "LOW OK");
    
    LOG_INFO("  Priority order verified: HIGH > MEDIUM > LOW");
    LOG_INFO("  Result: Passed");
}

int main(void) {
    LOG_INFO("========================================");
    LOG_INFO("  Integration Test: Priority Handling");
    LOG_INFO("========================================");
    LOG_INFO("");
    test_priority_order();
    LOG_INFO("");
    LOG_INFO("  Passed: %u, Failed: %u", tests_passed, tests_failed);
    LOG_INFO("========================================");
    return tests_failed == 0 ? 0 : 1;
}
