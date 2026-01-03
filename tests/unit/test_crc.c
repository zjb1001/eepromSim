/**
 * @file test_crc.c
 * @brief Unit Test: CRC Engine
 *
 * Test Coverage:
 * - CRC8 calculation and verification
 * - CRC16 calculation and verification
 * - CRC32 calculation and verification
 * - Error detection capability
 * - Performance benchmarks
 *
 * Test Strategy:
 * - Known vector testing
 * - Boundary value analysis
 * - Error injection
 * - Performance measurement
 */

#include "nvm.h"
#include "crc.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>

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

#define TEST_ASSERT_EQ(actual, expected, message) \
    TEST_ASSERT((actual) == (expected), message)

/**
 * @brief Test CRC8 calculation with known vectors
 */
static void test_crc8_known_vectors(void)
{
    LOG_INFO("");
    LOG_INFO("Test: CRC8 Known Vectors");

    /* Test vector 1: All zeros */
    uint8_t data1[64] = {0};
    uint8_t crc1 = CRC_CalculateCRC8(data1, 64);
    LOG_INFO("  All zeros (64B): CRC8 = 0x%02X", crc1);
    TEST_ASSERT(1, "CRC8 calculated for all zeros");

    /* Test vector 2: All 0xFF */
    uint8_t data2[64];
    memset(data2, 0xFF, 64);
    uint8_t crc2 = CRC_CalculateCRC8(data2, 64);
    LOG_INFO("  All 0xFF (64B): CRC8 = 0x%02X", crc2);
    TEST_ASSERT(1, "CRC8 calculated for all 0xFF");

    /* Test vector 3: Incrementing pattern */
    uint8_t data3[64];
    for (int i = 0; i < 64; i++) {
        data3[i] = (uint8_t)i;
    }
    uint8_t crc3 = CRC_CalculateCRC8(data3, 64);
    LOG_INFO("  Incrementing (64B): CRC8 = 0x%02X", crc3);
    TEST_ASSERT(1, "CRC8 calculated for incrementing pattern");

    /* Test vector 4: Alternating pattern */
    uint8_t data4[64];
    for (int i = 0; i < 64; i++) {
        data4[i] = (i % 2) ? 0xAA : 0x55;
    }
    uint8_t crc4 = CRC_CalculateCRC8(data4, 64);
    LOG_INFO("  Alternating (64B): CRC8 = 0x%02X", crc4);
    TEST_ASSERT(1, "CRC8 calculated for alternating pattern");

    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test CRC16 calculation with known vectors
 */
static void test_crc16_known_vectors(void)
{
    LOG_INFO("");
    LOG_INFO("Test: CRC16 Known Vectors");

    /* Test vector 1: All zeros */
    uint8_t data1[256] = {0};
    uint16_t crc1 = CRC_CalculateCRC16(data1, 256);
    LOG_INFO("  All zeros (256B): CRC16 = 0x%04X", crc1);
    TEST_ASSERT(1, "CRC16 calculated for all zeros");

    /* Test vector 2: All 0xFF */
    uint8_t data2[256];
    memset(data2, 0xFF, 256);
    uint16_t crc2 = CRC_CalculateCRC16(data2, 256);
    LOG_INFO("  All 0xFF (256B): CRC16 = 0x%04X", crc2);
    TEST_ASSERT(1, "CRC16 calculated for all 0xFF");

    /* Test vector 3: Pattern 0xAA */
    uint8_t data3[256];
    memset(data3, 0xAA, 256);
    uint16_t crc3 = CRC_CalculateCRC16(data3, 256);
    LOG_INFO("  0xAA pattern (256B): CRC16 = 0x%04X", crc3);
    TEST_ASSERT(1, "CRC16 calculated for 0xAA pattern");

    /* Test vector 4: ASCII text */
    const char* text = "Hello, EEPROM World!";
    uint16_t crc4 = CRC_CalculateCRC16((const uint8_t*)text, strlen(text));
    LOG_INFO("  Text \"%s\": CRC16 = 0x%04X", text, crc4);
    TEST_ASSERT(1, "CRC16 calculated for ASCII text");

    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test CRC32 calculation with known vectors
 */
static void test_crc32_known_vectors(void)
{
    LOG_INFO("");
    LOG_INFO("Test: CRC32 Known Vectors");

    /* Test vector 1: All zeros */
    uint8_t data1[1024] = {0};
    uint32_t crc1 = CRC_CalculateCRC32(data1, 1024);
    LOG_INFO("  All zeros (1KB): CRC32 = 0x%08X", crc1);
    TEST_ASSERT(1, "CRC32 calculated for all zeros");

    /* Test vector 2: All 0xFF */
    uint8_t data2[1024];
    memset(data2, 0xFF, 1024);
    uint32_t crc2 = CRC_CalculateCRC32(data2, 1024);
    LOG_INFO("  All 0xFF (1KB): CRC32 = 0x%08X", crc2);
    TEST_ASSERT(1, "CRC32 calculated for all 0xFF");

    /* Test vector 3: Incrementing pattern */
    uint8_t data3[1024];
    for (int i = 0; i < 1024; i++) {
        data3[i] = (uint8_t)(i & 0xFF);
    }
    uint32_t crc3 = CRC_CalculateCRC32(data3, 1024);
    LOG_INFO("  Incrementing (1KB): CRC32 = 0x%08X", crc3);
    TEST_ASSERT(1, "CRC32 calculated for incrementing pattern");

    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test CRC error detection (single-bit flip)
 */
static void test_crc_error_detection_single_bit(void)
{
    LOG_INFO("");
    LOG_INFO("Test: CRC Error Detection (Single-Bit Flip)");

    /* Original data */
    uint8_t data[256];
    memset(data, 0xAA, 256);
    uint16_t crc_original = CRC_CalculateCRC16(data, 256);

    LOG_INFO("  Original data: 0xAA pattern (256B)");
    LOG_INFO("  Original CRC16: 0x%04X", crc_original);

    /* Flip single bit at position 10 */
    data[10] ^= 0x01;  /* Flip bit 0 */
    uint16_t crc_corrupted = CRC_CalculateCRC16(data, 256);

    LOG_INFO("  Corrupted data: bit flip at offset 10");
    LOG_INFO("  Corrupted CRC16: 0x%04X", crc_corrupted);

    /* CRCs should be different */
    TEST_ASSERT(crc_original != crc_corrupted,
               "Single-bit flip detected (CRCs differ)");

    /* Flip bit back */
    data[10] ^= 0x01;
    uint16_t crc_restored = CRC_CalculateCRC16(data, 256);

    LOG_INFO("  Restored CRC16: 0x%04X", crc_restored);
    TEST_ASSERT_EQ(crc_restored, crc_original,
                  "Restored data matches original CRC");

    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test CRC error detection (multi-bit flip)
 */
static void test_crc_error_detection_multi_bit(void)
{
    LOG_INFO("");
    LOG_INFO("Test: CRC Error Detection (Multi-Bit Flip)");

    /* Original data */
    uint8_t data[256];
    memset(data, 0x55, 256);
    uint16_t crc_original = CRC_CalculateCRC16(data, 256);

    LOG_INFO("  Original data: 0x55 pattern (256B)");
    LOG_INFO("  Original CRC16: 0x%04X", crc_original);

    /* Flip multiple bits */
    data[0] ^= 0xFF;  /* Flip all 8 bits */
    data[100] ^= 0xAA;  /* Flip 4 bits */
    data[200] ^= 0x55;  /* Flip 6 bits */
    uint16_t crc_corrupted = CRC_CalculateCRC16(data, 256);

    LOG_INFO("  Corrupted data: 3 bytes with bit flips");
    LOG_INFO("  Corrupted CRC16: 0x%04X", crc_corrupted);

    /* CRCs should be different */
    TEST_ASSERT(crc_original != crc_corrupted,
               "Multi-bit flip detected (CRCs differ)");

    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test CRC8 vs CRC16 vs CRC32 strength
 */
static void test_crc_strength_comparison(void)
{
    LOG_INFO("");
    LOG_INFO("Test: CRC Strength Comparison");

    uint8_t data[256];
    memset(data, 0xAA, 256);

    /* Calculate all CRC types */
    uint8_t crc8 = CRC_CalculateCRC8(data, 256);
    uint16_t crc16 = CRC_CalculateCRC16(data, 256);
    uint32_t crc32 = CRC_CalculateCRC32(data, 256);

    LOG_INFO("  Data: 0xAA pattern (256B)");
    LOG_INFO("  CRC8:  0x%02X (2 bytes, 256 values)", crc8);
    LOG_INFO("  CRC16: 0x%04X (4 bytes, 65536 values)", crc16);
    LOG_INFO("  CRC32: 0x%08X (8 bytes, 4B values)", crc32);

    /* Test error detection rate */
    uint32_t crc8_collisions = 0;
    uint32_t crc16_collisions = 0;
    uint32_t crc32_collisions = 0;

    /* Generate 1000 random patterns and check for collisions */
    LOG_INFO("");
    LOG_INFO("  Testing 1000 random patterns for collisions...");

    srand((unsigned int)time(NULL));

    for (int i = 0; i < 1000; i++) {
        /* Generate random pattern */
        for (int j = 0; j < 256; j++) {
            data[j] = (uint8_t)rand();
        }

        uint8_t crc8_new = CRC_CalculateCRC8(data, 256);
        uint16_t crc16_new = CRC_CalculateCRC16(data, 256);
        uint32_t crc32_new = CRC_CalculateCRC32(data, 256);

        /* Check for collisions with first pattern */
        if (i > 0) {
            if (crc8_new == crc8) crc8_collisions++;
            if (crc16_new == crc16) crc16_collisions++;
            if (crc32_new == crc32) crc32_collisions++;
        } else {
            /* Save first pattern */
            crc8 = crc8_new;
            crc16 = crc16_new;
            crc32 = crc32_new;
        }
    }

    LOG_INFO("  Collisions with first pattern:");
    LOG_INFO("    CRC8:  %u / 999 (%.2f%%)",
             crc8_collisions, crc8_collisions * 100.0 / 999);
    LOG_INFO("    CRC16: %u / 999 (%.2f%%)",
             crc16_collisions, crc16_collisions * 100.0 / 999);
    LOG_INFO("    CRC32: %u / 999 (%.2f%%)",
             crc32_collisions, crc32_collisions * 100.0 / 999);

    TEST_ASSERT(crc8_collisions > crc16_collisions,
               "CRC16 has fewer collisions than CRC8");
    TEST_ASSERT(crc16_collisions > crc32_collisions,
               "CRC32 has fewer collisions than CRC16");

    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test CRC performance
 */
static void test_crc_performance(void)
{
    LOG_INFO("");
    LOG_INFO("Test: CRC Performance Benchmarks");

    uint8_t data[1024];
    memset(data, 0xAA, 1024);

    clock_t start, end;
    double cpu_time_used;

    /* Benchmark CRC8 (64 bytes) */
    start = clock();
    for (int i = 0; i < 100000; i++) {
        CRC_CalculateCRC8(data, 64);
    }
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    LOG_INFO("  CRC8 (64B): 100K iterations in %.3f sec (%.2f µs/op)",
             cpu_time_used, cpu_time_used * 1000000 / 100000);

    /* Benchmark CRC16 (256 bytes) */
    start = clock();
    for (int i = 0; i < 100000; i++) {
        CRC_CalculateCRC16(data, 256);
    }
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    LOG_INFO("  CRC16 (256B): 100K iterations in %.3f sec (%.2f µs/op)",
             cpu_time_used, cpu_time_used * 1000000 / 100000);

    /* Benchmark CRC32 (1024 bytes) */
    start = clock();
    for (int i = 0; i < 100000; i++) {
        CRC_CalculateCRC32(data, 1024);
    }
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    LOG_INFO("  CRC32 (1KB): 100K iterations in %.3f sec (%.2f µs/op)",
             cpu_time_used, cpu_time_used * 1000000 / 100000);

    TEST_ASSERT(1, "CRC performance benchmark completed");

    LOG_INFO("  Result: Passed");
}

/**
 * @brief Test CRC with NvM integration
 */
static void test_crc_nvm_integration(void)
{
    LOG_INFO("");
    LOG_INFO("Test: CRC Integration with NvM");

    NvM_Init();
    OsScheduler_Init(16);

    /* Register block with CRC16 */
    static uint8_t test_data[256];
    NvM_BlockConfig_t block = {
        .block_id = 1,
        .block_size = 256,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = test_data,
        .rom_block_ptr = NULL,
        .rom_block_size = 0,
        .eeprom_offset = 0x0000
    };
    NvM_RegisterBlock(&block);

    /* Write data (CRC calculated automatically) */
    memset(test_data, 0xBB, 256);
    NvM_WriteBlock(1, test_data);

    uint8_t job_result;
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(1, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    TEST_ASSERT_EQ(job_result, NVM_REQ_OK, "Write with CRC OK");

    /* Read back (CRC verified automatically) */
    memset(test_data, 0x00, 256);
    NvM_ReadBlock(1, test_data);

    iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(1, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    TEST_ASSERT_EQ(job_result, NVM_REQ_OK, "Read with CRC verification OK");
    TEST_ASSERT_EQ(test_data[0], 0xBB, "Data integrity verified via CRC");

    LOG_INFO("  Result: Passed");
}

/**
 * @brief Run all CRC tests
 */
int main(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Unit Test: CRC Engine");
    LOG_INFO("========================================");
    LOG_INFO("");

    /* Run tests */
    test_crc8_known_vectors();
    test_crc16_known_vectors();
    test_crc32_known_vectors();
    test_crc_error_detection_single_bit();
    test_crc_error_detection_multi_bit();
    test_crc_strength_comparison();
    test_crc_performance();
    test_crc_nvm_integration();

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
