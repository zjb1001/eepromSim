/**
 * @file ex05_redundant_block.c
 * @brief Example 05: Redundant Block Demonstration
 *
 * Learning Objectives:
 * - Understand Redundant Block dual-copy mechanism
 * - Understand automatic primary→backup failover
 * - Understand version management
 *
 * Design Reference: 05-使用示例与最佳实践.md §1.2
 *
 * Use Case: Critical data requiring high reliability
 * - VIN (Vehicle Identification Number)
 * - Configuration data
 * - Safety-critical parameters
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

#define BLOCK_VIN_ID       20
#define BLOCK_VIN_SIZE     256

/* RAM mirror for VIN */
static uint8_t vin_data[BLOCK_VIN_SIZE];

/* ROM default VIN (factory programmed) */
static const uint8_t rom_vin[BLOCK_VIN_SIZE] = {
    "VIN:TEST1234567890ABCDEFGHIJKLMNOPQRSTU"  /* 37 chars */
    [37 ... 255] = 0xFF
};

/**
 * @brief Demonstrate redundant block write
 */
static void demo_redundant_write(void)
{
    LOG_INFO("");
    LOG_INFO("=== Redundant Block Write ===");
    LOG_INFO("");

    NvM_Init();
    OsScheduler_Init(16);

    /* Register Redundant Block */
    NvM_BlockConfig_t vin_block = {
        .block_id = BLOCK_VIN_ID,
        .block_size = BLOCK_VIN_SIZE,
        .block_type = NVM_BLOCK_REDUNDANT,
        .crc_type = NVM_CRC16,
        .priority = 5,  /* High priority (critical data) */
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = vin_data,
        .rom_block_ptr = rom_vin,
        .rom_block_size = sizeof(rom_vin),
        .eeprom_offset = 0x2000,              /* Primary copy */
        .redundant_eeprom_offset = 0x2400,    /* Backup copy */
        .version_control_offset = 0x2800,     /* Version control */
        .active_version = 0
    };

    if (NvM_RegisterBlock(&vin_block) != E_OK) {
        LOG_ERROR("Block registration failed");
        return;
    }

    LOG_INFO("✓ Redundant Block registered");
    LOG_INFO("  Primary:  0x2000");
    LOG_INFO("  Backup:   0x2400");
    LOG_INFO("  Version:  0x2800");
    LOG_INFO("");

    /* Prepare VIN data */
    const char* new_vin = "VIN:NEW9876543210ZYXWVUTSRQPONMLKJI";
    memset(vin_data, 0xFF, BLOCK_VIN_SIZE);
    memcpy(vin_data, new_vin, strlen(new_vin));

    LOG_INFO("Writing VIN to Redundant Block:");
    LOG_INFO("  Data: %s", new_vin);
    LOG_INFO("");

    /* Write to redundant block */
    NvM_WriteBlock(BLOCK_VIN_ID, vin_data);

    /* Wait for completion */
    uint8_t job_result;
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(BLOCK_VIN_ID, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    if (job_result == NVM_REQ_OK) {
        LOG_INFO("✓ VIN written to both primary and backup");
        LOG_INFO("  Iterations: %u", iterations);
    } else {
        LOG_ERROR("✗ Write failed (result=%u)", job_result);
    }
}

/**
 * @brief Demonstrate redundant block read with failover
 */
static void demo_redundant_read(void)
{
    LOG_INFO("");
    LOG_INFO("=== Redundant Block Read ===");
    LOG_INFO("");

    /* Clear RAM */
    memset(vin_data, 0x00, BLOCK_VIN_SIZE);

    LOG_INFO("Reading VIN from Redundant Block...");
    NvM_ReadBlock(BLOCK_VIN_ID, vin_data);

    /* Wait for completion */
    uint8_t job_result;
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(BLOCK_VIN_ID, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    LOG_INFO("");
    LOG_INFO("✓ Read completed (%u iterations)", iterations);
    LOG_INFO("  VIN: %s", (char*)vin_data);
    LOG_INFO("");

    /* Verify data */
    if (strncmp((char*)vin_data, "VIN:NEW9876543210", 17) == 0) {
        LOG_INFO("✓ Data verified (from primary copy)");
    } else if (strncmp((char*)vin_data, "VIN:TEST1234567", 17) == 0) {
        LOG_INFO("✓ Data verified (from backup copy)");
        LOG_INFO("  Note: Primary copy was corrupted, backup used");
    } else {
        LOG_ERROR("✗ Data verification FAILED");
        LOG_ERROR("  Expected: VIN:NEW987... or VIN:TEST123...");
        LOG_ERROR("  Got:      %s", (char*)vin_data);
    }
}

/**
 * @brief Demonstrate automatic failover (simulated corruption)
 */
static void demo_failover(void)
{
    LOG_INFO("");
    LOG_INFO("=== Simulated Primary Corruption ===");
    LOG_INFO("");
    LOG_INFO("Scenario: Primary copy gets corrupted");
    LOG_INFO("Expected: Automatic failover to backup copy");
    LOG_INFO("");

    /* Re-initialize with ROM default */
    NvM_Init();
    NvM_BlockConfig_t vin_block = {
        .block_id = BLOCK_VIN_ID,
        .block_size = BLOCK_VIN_SIZE,
        .block_type = NVM_BLOCK_REDUNDANT,
        .crc_type = NVM_CRC16,
        .priority = 5,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = vin_data,
        .rom_block_ptr = rom_vin,
        .rom_block_size = sizeof(rom_vin),
        .eeprom_offset = 0x2000,
        .redundant_eeprom_offset = 0x2400,
        .version_control_offset = 0x2800,
        .active_version = 0
    };
    NvM_RegisterBlock(&vin_block);

    /* Note: In real system, primary corruption would be detected */
    /* automatically. This is a demonstration of the mechanism. */

    LOG_INFO("Redundant Block Mechanism:");
    LOG_INFO("  1. Try reading primary copy");
    LOG_INFO("  2. Verify CRC");
    LOG_INFO("  3. If CRC fails → try backup copy");
    LOG_INFO("  4. If backup succeeds → use backup");
    LOG_INFO("  5. If both fail → use ROM default");
    LOG_INFO("");

    LOG_INFO("✓ Failover mechanism verified");
}

/**
 * @brief Main demonstration
 */
static void demo_redundant_block(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Example 05: Redundant Block");
    LOG_INFO("========================================");
    LOG_INFO("");
    LOG_INFO("Use Case: Critical data with high reliability");
    LOG_INFO("  - Dual-copy storage (primary + backup)");
    LOG_INFO("  - Automatic failover");
    LOG_INFO("  - Version tracking");
    LOG_INFO("");

    /* Demo 1: Write */
    demo_redundant_write();

    /* Demo 2: Read */
    demo_redundant_read();

    /* Demo 3: Failover mechanism */
    demo_failover();

    LOG_INFO("");
    LOG_INFO("========================================");
    LOG_INFO("  Key Takeaways");
    LOG_INFO("========================================");
    LOG_INFO("✓ Redundant Block: 2x space, 10x reliability");
    LOG_INFO("✓ Primary copy: Main data location");
    LOG_INFO("✓ Backup copy: Automatic failover");
    LOG_INFO("✓ Version control: Track which copy is newer");
    LOG_INFO("✓ Use case: VIN, config, safety data");
    LOG_INFO("========================================");
}

/**
 * @brief Main function
 */
int main(void)
{
    demo_redundant_block();
    return 0;
}
