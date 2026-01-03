/**
 * @file test_read_all.c
 * @brief Integration Test: ReadAll Functionality
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

static void test_read_all_multiple_blocks(void) {
    LOG_INFO("Test: ReadAll with Multiple Blocks");
    
    NvM_Init();
    OsScheduler_Init(16);
    
    static uint8_t data1[256], data2[256], data3[256];
    static const uint8_t rom1[256] = { ['A'] = 1, [1 ... 255] = 0xFF };
    static const uint8_t rom2[256] = { ['B'] = 2, [1 ... 255] = 0xFF };
    static const uint8_t rom3[256] = { ['C'] = 3, [1 ... 255] = 0xFF };
    
    NvM_BlockConfig_t block1 = {
        .block_id = 1, .block_size = 256, .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16, .priority = 5, .is_immediate = FALSE,
        .is_write_protected = FALSE, .ram_mirror_ptr = data1,
        .rom_block_ptr = rom1, .rom_block_size = sizeof(rom1), .eeprom_offset = 0x1000
    };
    NvM_RegisterBlock(&block1);
    
    NvM_BlockConfig_t block2 = {
        .block_id = 2, .block_size = 256, .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16, .priority = 10, .is_immediate = FALSE,
        .is_write_protected = FALSE, .ram_mirror_ptr = data2,
        .rom_block_ptr = rom2, .rom_block_size = sizeof(rom2), .eeprom_offset = 0x2000
    };
    NvM_RegisterBlock(&block2);
    
    NvM_BlockConfig_t block3 = {
        .block_id = 3, .block_size = 256, .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16, .priority = 15, .is_immediate = FALSE,
        .is_write_protected = FALSE, .ram_mirror_ptr = data3,
        .rom_block_ptr = rom3, .rom_block_size = sizeof(rom3), .eeprom_offset = 0x3000
    };
    NvM_RegisterBlock(&block3);
    
    NvM_ReadAll();
    
    uint8_t result1, result2, result3;
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(1, &result1);
        NvM_GetJobResult(2, &result2);
        NvM_GetJobResult(3, &result3);
        iterations++;
        if (iterations > 500) break;
    } while (result1 == NVM_REQ_PENDING || result2 == NVM_REQ_PENDING || result3 == NVM_REQ_PENDING);
    
    TEST_ASSERT(result1 == NVM_REQ_OK, "Block 1 loaded");
    TEST_ASSERT(result2 == NVM_REQ_OK, "Block 2 loaded");
    TEST_ASSERT(result3 == NVM_REQ_OK, "Block 3 loaded");
    TEST_ASSERT(data1[0] == 'A', "ROM 1 loaded");
    TEST_ASSERT(data2[0] == 'B', "ROM 2 loaded");
    TEST_ASSERT(data3[0] == 'C', "ROM 3 loaded");
    
    LOG_INFO("  Result: Passed");
}

int main(void) {
    LOG_INFO("========================================");
    LOG_INFO("  Integration Test: ReadAll");
    LOG_INFO("========================================");
    LOG_INFO("");
    
    test_read_all_multiple_blocks();
    
    LOG_INFO("");
    LOG_INFO("========================================");
    LOG_INFO("  Passed: %u, Failed: %u", tests_passed, tests_failed);
    LOG_INFO("========================================");
    
    return tests_failed == 0 ? 0 : 1;
}
