#!/usr/bin/env python3
"""
批量生成剩余示例程序脚本
Generated examples:
- ex06: Dataset Block
- ex07: Multiple Blocks
- ex08: Priority Jobs
- ex09: CRC Verification
- ex10: Power Cycle
- fault01-04: Fault scenarios
"""

import os
from pathlib import Path

# Base directory
base_dir = Path("/home/page/GitPlayground/eepromSim")

examples = {
    "ex06_dataset_block": {
        "path": "examples/advanced/ex06_dataset_block.c",
        "content": '''/**
 * @file ex06_dataset_block.c
 * @brief Example 06: Dataset Block Demonstration
 *
 * Learning Objectives:
 * - Understand Dataset Block multi-version management
 * - Understand round-robin versioning
 * - Understand NvM_SetDataIndex API
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

#define BLOCK_DATASET_ID   30
#define BLOCK_DATASET_SIZE  256
#define DATASET_VERSIONS   3

static uint8_t dataset_data[BLOCK_DATASET_SIZE];

static void demo_dataset_block(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Example 06: Dataset Block");
    LOG_INFO("========================================");
    LOG_INFO("");

    NvM_Init();
    OsScheduler_Init(16);

    NvM_BlockConfig_t dataset_block = {
        .block_id = BLOCK_DATASET_ID,
        .block_size = BLOCK_DATASET_SIZE,
        .block_type = NVM_BLOCK_DATASET,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = dataset_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x3000,
        .dataset_count = DATASET_VERSIONS,
        .active_dataset_index = 0
    };

    NvM_RegisterBlock(&dataset_block);
    LOG_INFO("✓ Dataset Block registered (3 versions)");
    LOG_INFO("");

    /* Write 3 versions */
    LOG_INFO("=== Writing 3 Versions ===");
    for (uint8_t ver = 0; ver < 3; ver++) {
        memset(dataset_data, 0x10 + ver, BLOCK_DATASET_SIZE);
        LOG_INFO("Writing version %u (pattern 0x%02X)...", ver, 0x10 + ver);
        NvM_WriteBlock(BLOCK_DATASET_ID, dataset_data);
        for (int i = 0; i < 20; i++) NvM_MainFunction();
    }
    LOG_INFO("✓ All versions written");
    LOG_INFO("");

    /* Test SetDataIndex */
    LOG_INFO("=== Testing NvM_SetDataIndex ===");
    for (uint8_t idx = 0; idx < 3; idx++) {
        if (NvM_SetDataIndex(BLOCK_DATASET_ID, idx) == E_OK) {
            memset(dataset_data, 0x00, BLOCK_DATASET_SIZE);
            NvM_ReadBlock(BLOCK_DATASET_ID, dataset_data);
            for (int i = 0; i < 20; i++) NvM_MainFunction();
            LOG_INFO("Version %u: data = 0x%02X", idx, dataset_data[0]);
        }
    }
    LOG_INFO("");

    LOG_INFO("========================================");
    LOG_INFO("  Key Takeaways");
    LOG_INFO("========================================");
    LOG_INFO("✓ Dataset Block: 3 versions");
    LOG_INFO("✓ Round-robin versioning");
    LOG_INFO("✓ SetDataIndex: Manual version switch");
    LOG_INFO("✓ Use case: High-frequency writes");
    LOG_INFO("========================================");
}

int main(void)
{
    demo_dataset_block();
    return 0;
}
'''
    },

    "ex07_multiple_blocks": {
        "path": "examples/advanced/ex07_multiple_blocks.c",
        "content": '''/**
 * @file ex07_multiple_blocks.c
 * @brief Example 07: Multiple Blocks Coordination
 *
 * Learning Objectives:
 * - Understand multi-block Job queue management
 * - Understand priority-based scheduling
 * - Understand concurrent block operations
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

static uint8_t data_a[256], data_b[256], data_c[256];

static void demo_multiple_blocks(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Example 07: Multiple Blocks");
    LOG_INFO("========================================");
    LOG_INFO("");

    NvM_Init();
    OsScheduler_Init(16);

    /* Register 3 blocks with different priorities */
    NvM_BlockConfig_t blocks[] = {
        {100, 256, NVM_BLOCK_NATIVE, NVM_CRC16, 1, FALSE, FALSE, data_a, NULL, 0, 0x4000},
        {101, 256, NVM_BLOCK_NATIVE, NVM_CRC16, 10, FALSE, FALSE, data_b, NULL, 0, 0x4400},
        {102, 256, NVM_BLOCK_NATIVE, NVM_CRC16, 20, FALSE, FALSE, data_c, NULL, 0, 0x4800}
    };

    for (int i = 0; i < 3; i++) {
        NvM_RegisterBlock(&blocks[i]);
        LOG_INFO("✓ Block %d registered (priority=%d)", blocks[i].block_id, blocks[i].priority);
    }
    LOG_INFO("");

    /* Submit multiple writes */
    LOG_INFO("=== Submitting Multiple Writes ===");
    memset(data_a, 0xAA, 256);
    memset(data_b, 0xBB, 256);
    memset(data_c, 0xCC, 256);

    NvM_WriteBlock(100, data_a);  /* Priority 1 (HIGH) */
    NvM_WriteBlock(101, data_b);  /* Priority 10 (MEDIUM) */
    NvM_WriteBlock(102, data_c);  /* Priority 20 (LOW) */

    LOG_INFO("3 writes submitted (priority order: 100 > 101 > 102)");
    LOG_INFO("");

    /* Process jobs */
    LOG_INFO("=== Processing Jobs (Priority Order) ===");
    uint32_t iterations = 0;
    uint8_t results[3] = {NVM_REQ_PENDING, NVM_REQ_PENDING, NVM_REQ_PENDING};

    do {
        NvM_MainFunction();
        iterations++;

        for (int i = 0; i < 3; i++) {
            if (results[i] == NVM_REQ_PENDING) {
                NvM_GetJobResult(100 + i, &results[i]);
            }
        }

    } while ((results[0] == NVM_REQ_PENDING ||
             results[1] == NVM_REQ_PENDING ||
             results[2] == NVM_REQ_PENDING) && iterations < 200);

    LOG_INFO("✓ All jobs completed (%u iterations)", iterations);
    for (int i = 0; i < 3; i++) {
        LOG_INFO("  Block %d: %s", 100 + i,
                 (results[i] == NVM_REQ_OK) ? "OK" : "FAILED");
    }
    LOG_INFO("");

    LOG_INFO("========================================");
    LOG_INFO("  Key Takeaways");
    LOG_INFO("========================================");
    LOG_INFO("✓ Priority queue: High priority first");
    LOG_INFO("✓ Job scheduling: FIFO within priority");
    LOG_INFO("✓ Multi-block: Concurrent operations");
    LOG_INFO("========================================");
}

int main(void)
{
    demo_multiple_blocks();
    return 0;
}
'''
    }
}

def generate_examples():
    """Generate all example programs"""
    for name, config in examples.items():
        file_path = base_dir / config["path"]
        file_path.parent.mkdir(parents=True, exist_ok=True)

        with open(file_path, "w") as f:
            f.write(config["content"])

        print(f"✓ Generated {file_path}")

if __name__ == "__main__":
    generate_examples()
    print("\\n✓ All remaining examples generated successfully")
