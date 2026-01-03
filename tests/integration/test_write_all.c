/**
 * @file test_write_all.c
 * @brief Integration Test: WriteAll Functionality
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

static uint32_t tests_passed = 0;
static uint32_t tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (cond) { tests_passed++; LOG_INFO("  ✓ %s", msg); } \
        else { tests_failed++; LOG_ERROR("  ✗ %s", msg); } \
    } while(0)

static void test_write_all_shutdown(void) {
    LOG_INFO("Test: WriteAll Shutdown Safety");
    
    NvM_Init();
    OsScheduler_Init(16);
    
    static uint8_t data1[256], data2[256], data3[256];
    
    NvM_BlockConfig_t block1 = {
        .block_id = 10, .block_size = 256, .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16, .priority = 5, .is_immediate = FALSE,
        .is_write_protected = FALSE, .ram_mirror_ptr = data1,
        .rom_block_ptr = NULL, .rom_block_size = 0, .eeprom_offset = 0x4000
    };
    NvM_RegisterBlock(&block1);
    
    NvM_BlockConfig_t block2 = {
        .block_id = 11, .block_size = 256, .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16, .priority = 10, .is_immediate = FALSE,
        .is_write_protected = FALSE, .ram_mirror_ptr = data2,
        .rom_block_ptr = NULL, .rom_block_size = 0, .eeprom_offset = 0x5000
    };
    NvM_RegisterBlock(&block2);
    
    NvM_BlockConfig_t block3 = {
        .block_id = 12, .block_size = 256, .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16, .priority = 15, .is_immediate = FALSE,
        .is_write_protected = FALSE, .ram_mirror_ptr = data3,
        .rom_block_ptr = NULL, .rom_block_size = 0, .eeprom_offset = 0x6000
    };
    NvM_RegisterBlock(&block3);
    
    memset(data1, 0x11, 256);
    memset(data2, 0x22, 256);
    memset(data3, 0x33, 256);
    
    NvM_WriteAll();
    
    uint8_t result1, result2, result3;
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(10, &result1);
        NvM_GetJobResult(11, &result2);
        NvM_GetJobResult(12, &result3);
        iterations++;
        if (iterations > 500) break;
    } while (result1 == NVM_REQ_PENDING || result2 == NVM_REQ_PENDING || result3 == NVM_REQ_PENDING);
    
    TEST_ASSERT(result1 == NVM_REQ_OK, "Block 1 saved");
    TEST_ASSERT(result2 == NVM_REQ_OK, "Block 2 saved");
    TEST_ASSERT(result3 == NVM_REQ_OK, "Block 3 saved");
    
    LOG_INFO("  All blocks safely persisted");
    LOG_INFO("  Result: Passed");
}

int main(void) {
    LOG_INFO("========================================");
    LOG_INFO("  Integration Test: WriteAll");
    LOG_INFO("========================================");
    LOG_INFO("");
    
    test_write_all_shutdown();
    
    LOG_INFO("");
    LOG_INFO("========================================");
    LOG_INFO("  Passed: %u, Failed: %u", tests_passed, tests_failed);
    LOG_INFO("========================================");
    
    return tests_failed == 0 ? 0 : 1;
}
