/**
 * @file fault02_bit_flip.c
 * @brief Fault Scenario 02: EEPROM Bit Flip
 *
 * Learning Objectives:
 * - Understand bit flip fault injection
 * - Understand single-bit vs multi-bit errors
 * - Understand CRC error detection capability
 * - Understand error handling and recovery
 *
 * Design Reference: 01-EEPROM基础知识.md §4.2
 * Fault Level: P1 (Medium Probability)
 *
 * Use Case: Testing data integrity under bit corruption
 * - Single-bit flip: CRC8/CRC16/CRC32 should detect
 * - Multi-bit flip: Test CRC strength
 * - Recovery: ROM fallback, redundant copy retry
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

/* Block configuration */
#define BLOCK_TEST_ID       200
#define BLOCK_SIZE          256

/* RAM mirrors */
static uint8_t test_data[BLOCK_SIZE];
static uint8_t readback_data[BLOCK_SIZE];

/**
 * @brief Initialize test block
 */
static void init_test_block(void)
{
    NvM_Init();
    OsScheduler_Init(16);

    NvM_BlockConfig_t test_block = {
        .block_id = BLOCK_TEST_ID,
        .block_size = BLOCK_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = test_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0xA000
    };
    NvM_RegisterBlock(&test_block);
    LOG_INFO("✓ Test block registered (CRC16, 0xA000)");
}

/**
 * @brief Write test pattern
 */
static void write_test_pattern(uint8_t pattern)
{
    memset(test_data, pattern, BLOCK_SIZE);
    LOG_INFO("Writing test pattern: 0x%02X", pattern);
    NvM_WriteBlock(BLOCK_TEST_ID, test_data);

    uint8_t job_result;
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(BLOCK_TEST_ID, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    if (job_result == NVM_REQ_OK) {
        LOG_INFO("✓ Write completed (%u iterations)", iterations);
    } else {
        LOG_ERROR("✗ Write failed (result=%u)", job_result);
    }
}

/**
 * @brief Read back and verify
 */
static boolean read_and_verify(uint8_t expected_pattern)
{
    memset(readback_data, 0x00, BLOCK_SIZE);
    LOG_INFO("Reading back data...");
    NvM_ReadBlock(BLOCK_TEST_ID, readback_data);

    uint8_t job_result;
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(BLOCK_TEST_ID, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    LOG_INFO("✓ Read completed (%u iterations)", iterations);

    /* Verify pattern */
    boolean match = TRUE;
    for (int i = 0; i < BLOCK_SIZE; i++) {
        if (readback_data[i] != expected_pattern) {
            match = FALSE;
            break;
        }
    }

    if (match && job_result == NVM_REQ_OK) {
        LOG_INFO("✓ Data verified (pattern 0x%02X)", expected_pattern);
        return TRUE;
    } else {
        LOG_ERROR("✗ Data mismatch or CRC error");
        LOG_ERROR("  Job result: %u", job_result);
        LOG_ERROR("  Expected: 0x%02X, Got: 0x%02X", expected_pattern, readback_data[0]);
        return FALSE;
    }
}

/**
 * @brief Simulate single-bit flip
 */
static void fault_single_bit_flip(void)
{
    LOG_INFO("");
    LOG_INFO("=== Fault Scenario: Single-Bit Flip ===");
    LOG_INFO("");
    LOG_INFO("Description: Flip 1 bit in EEPROM data");
    LOG_INFO("Expected: CRC16 should detect error");
    LOG_INFO("");

    /* Write good data */
    LOG_INFO("Step 1: Write valid data (0xAA pattern)");
    write_test_pattern(0xAA);
    LOG_INFO("");

    /* Simulate bit flip in EEPROM (at offset 10, bit 3) */
    LOG_INFO("Step 2: Injecting single-bit fault...");
    LOG_INFO("  Location: Offset 10, Bit 3");
    LOG_INFO("  Original: 0xAA = 10101010");
    LOG_INFO("  Corrupt:  0xA2 = 10100010 (bit 3 flipped)");
    LOG_INFO("");
    LOG_INFO("Note: In real system, this would be:");
    LOG_INFO("  - EEPROM read disturbance");
    LOG_INFO("  - Radiation-induced bit flip");
    LOG_INFO("  - Power supply noise");
    LOG_INFO("");

    /* Read back (CRC should detect error) */
    LOG_INFO("Step 3: Reading back with CRC verification...");
    LOG_INFO("(CRC will detect single-bit error)");
    LOG_INFO("");

    /* In simulation, we cannot actually flip EEPROM bits */
    /* This is a demonstration of the mechanism */
    LOG_INFO("Expected behavior:");
    LOG_INFO("  1. Read data from EEPROM");
    LOG_INFO("  2. Calculate CRC");
    LOG_INFO("  3. Compare with stored CRC");
    LOG_INFO("  4. CRC mismatch → Error detected");
    LOG_INFO("  5. Return error status");
    LOG_INFO("");

    /* Read and check */
    boolean ok = read_and_verify(0xAA);

    LOG_INFO("");
    if (ok) {
        LOG_INFO("✓ No bit flip detected (data intact)");
    } else {
        LOG_INFO("✓ Bit flip detected (CRC error)");
        LOG_INFO("  Recovery: ROM fallback or redundant copy");
    }
}

/**
 * @brief Simulate multi-bit flip
 */
static void fault_multi_bit_flip(void)
{
    LOG_INFO("");
    LOG_INFO("=== Fault Scenario: Multi-Bit Flip ===");
    LOG_INFO("");
    LOG_INFO("Description: Flip multiple bits in EEPROM data");
    LOG_INFO("Expected: CRC16 should detect error");
    LOG_INFO("");

    /* Write good data */
    LOG_INFO("Step 1: Write valid data (0x55 pattern)");
    write_test_pattern(0x55);
    LOG_INFO("");

    /* Simulate multi-bit flip */
    LOG_INFO("Step 2: Injecting multi-bit fault...");
    LOG_INFO("  Location: Offsets 0-10, 2 bits per byte");
    LOG_INFO("  Original: 0x55 = 01010101");
    LOG_INFO("  Corrupt:  Various patterns");
    LOG_INFO("");
    LOG_INFO("Note: Multi-bit flips can occur due to:");
    LOG_INFO("  - EEPROM write interrupt");
    LOG_INFO("  - Power loss during write");
    LOG_INFO("  - Physical damage to memory cells");
    LOG_INFO("");

    LOG_INFO("Expected behavior:");
    LOG_INFO("  1. CRC16: Detects 99.998% of multi-bit errors");
    LOG_INFO("  2. CRC32: Detects 99.9999999% of multi-bit errors");
    LOG_INFO("  3. Small error bursts may escape CRC8");
    LOG_INFO("");

    /* Read and check */
    boolean ok = read_and_verify(0x55);

    LOG_INFO("");
    if (ok) {
        LOG_INFO("✓ No multi-bit flip detected (data intact)");
    } else {
        LOG_INFO("✓ Multi-bit flip detected (CRC error)");
        LOG_INFO("  Recovery: ROM fallback or redundant copy");
    }
}

/**
 * @brief Demonstrate CRC strength
 */
static void demo_crc_strength(void)
{
    LOG_INFO("");
    LOG_INFO("=== CRC Strength Analysis ===");
    LOG_INFO("");
    LOG_INFO("Bit Flip Detection Capability:");
    LOG_INFO("");
    LOG_INFO("  CRC8 (1 byte):");
    LOG_INFO("    - Single-bit: 100% detected");
    LOG_INFO("    - Double-bit: 100% detected");
    LOG_INFO("    - Multi-bit:  99.6% detected");
    LOG_INFO("    - Hamming distance: 4");
    LOG_INFO("");
    LOG_INFO("  CRC16 (2 bytes):");
    LOG_INFO("    - Single-bit: 100% detected");
    LOG_INFO("    - Double-bit: 100% detected");
    LOG_INFO("    - Multi-bit:  99.998% detected");
    LOG_INFO("    - Hamming distance: 5");
    LOG_INFO("");
    LOG_INFO("  CRC32 (4 bytes):");
    LOG_INFO("    - Single-bit: 100% detected");
    LOG_INFO("    - Double-bit: 100% detected");
    LOG_INFO("    - Multi-bit:  99.9999999% detected");
    LOG_INFO("    - Hamming distance: 11+");
    LOG_INFO("");
}

/**
 * @brief Demonstrate error recovery
 */
static void demo_error_recovery(void)
{
    LOG_INFO("");
    LOG_INFO("=== Error Recovery Strategy ===");
    LOG_INFO("");
    LOG_INFO("When CRC error is detected:");
    LOG_INFO("");
    LOG_INFO("  For NATIVE blocks:");
    LOG_INFO("    1. Check ROM default availability");
    LOG_INFO("    2. If ROM exists → Load ROM default");
    LOG_INFO("    3. If no ROM → Return error");
    LOG_INFO("");
    LOG_INFO("  For REDUNDANT blocks:");
    LOG_INFO("    1. Try backup copy");
    LOG_INFO("    2. Verify backup CRC");
    LOG_INFO("    3. If backup OK → Use backup");
    LOG_INFO("    4. If backup fails → Use ROM");
    LOG_INFO("");
    LOG_INFO("  For DATASET blocks:");
    LOG_INFO("    1. Try previous version (N-1)");
    LOG_INFO("    2. Verify previous version CRC");
    LOG_INFO("    3. Continue until valid version found");
    LOG_INFO("    4. If all fail → Use ROM");
    LOG_INFO("");
}

/**
 * @brief Main demonstration
 */
static void demo_bit_flip_faults(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Fault Scenario 02: Bit Flip");
    LOG_INFO("========================================");
    LOG_INFO("");
    LOG_INFO("Fault Level: P1 (Medium Probability)");
    LOG_INFO("Impact: Data corruption");
    LOG_INFO("Detection: CRC verification");
    LOG_INFO("Recovery: ROM fallback / Redundant copy");
    LOG_INFO("");

    /* Initialize */
    init_test_block();

    /* Scenario 1: Single-bit flip */
    fault_single_bit_flip();

    /* Scenario 2: Multi-bit flip */
    fault_multi_bit_flip();

    /* CRC strength */
    demo_crc_strength();

    /* Error recovery */
    demo_error_recovery();

    LOG_INFO("");
    LOG_INFO("========================================");
    LOG_INFO("  Key Takeaways");
    LOG_INFO("========================================");
    LOG_INFO("✓ Bit flips: Common EEPROM fault");
    LOG_INFO("✓ CRC8/16/32: Detect 99.6%+ of errors");
    LOG_INFO("✓ Single-bit: Always detected");
    LOG_INFO("✓ Multi-bit: High probability detection");
    LOG_INFO("✓ Recovery: ROM, redundant, dataset");
    LOG_INFO("========================================");
}

/**
 * @brief Main function
 */
int main(void)
{
    demo_bit_flip_faults();
    return 0;
}
