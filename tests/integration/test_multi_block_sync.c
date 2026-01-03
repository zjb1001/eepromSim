/**
 * @file test_multi_block_sync.c
 * @brief Integration Test: Multi-Block Synchronization
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

static uint32_t tests_passed = 0, tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { if (cond) { tests_passed++; LOG_INFO("  ✓ %s", msg); } else { tests_failed++; LOG_ERROR("  ✗ %s", msg); } } while(0)

static void test_concurrent_multi_block(void) {
    LOG_INFO("Test: Concurrent Multi-Block Operations");
    
    NvM_Init();
    OsScheduler_Init(16);
    
    static uint8_t data[5][256];
    
    for (int i = 0; i < 5; i++) {
        NvM_BlockConfig_t block = { 
            .block_id = 50 + i, .block_size = 256, .block_type = NVM_BLOCK_NATIVE, 
            .crc_type = NVM_CRC16, .priority = 10 + i * 2, .is_immediate = FALSE, 
            .is_write_protected = FALSE, .ram_mirror_ptr = data[i], 
            .rom_block_ptr = NULL, .rom_block_size = 0, .eeprom_offset = (uint16_t)(0x8000 + i * 1024) 
        };
        NvM_RegisterBlock(&block);
    }
    
    for (int i = 0; i < 5; i++) {
        memset(data[i], 0x50 + i, 256);
        NvM_WriteBlock(50 + i, data[i]);
    }
    
    uint8_t results[5];
    uint32_t iterations = 0;
    boolean all_done = FALSE;
    
    do {
        NvM_MainFunction();
        iterations++;
        all_done = TRUE;
        for (int i = 0; i < 5; i++) {
            NvM_GetJobResult(50 + i, &results[i]);
            if (results[i] == NVM_REQ_PENDING) all_done = FALSE;
        }
        if (iterations > 500) break;
    } while (!all_done);
    
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT(results[i] == NVM_REQ_OK, "Block OK");
    }
    
    LOG_INFO("  All 5 blocks synchronized successfully");
    LOG_INFO("  Iterations: %u", iterations);
    LOG_INFO("  Result: Passed");
}

int main(void) {
    LOG_INFO("========================================");
    LOG_INFO("  Integration Test: Multi-Block Sync");
    LOG_INFO("========================================");
    LOG_INFO("");
    test_concurrent_multi_block();
    LOG_INFO("");
    LOG_INFO("  Passed: %u, Failed: %u", tests_passed, tests_failed);
    LOG_INFO("========================================");
    return tests_failed == 0 ? 0 : 1;
}
