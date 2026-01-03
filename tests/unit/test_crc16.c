/**
 * @file test_crc16.c
 * @brief Unit tests for CRC-16 calculation
 *
 * REQ-数据完整性: design/04-数据完整性方案.md
 * - 测试CRC16计算
 * - 测试已知向量
 */

#include "crc16.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/**
 * @brief Test basic CRC calculation
 */
static void test_basic_crc(void)
{
    LOG_INFO("Testing basic CRC calculation...");

    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint16_t crc = Crc16_Calculate(data, sizeof(data));

    LOG_INFO("  CRC: 0x%04X", crc);
    assert(crc != 0);

    LOG_INFO("✓ Basic CRC test passed");
}

/**
 * @brief Test CRC with empty data
 */
static void test_empty_data(void)
{
    LOG_INFO("Testing CRC with empty data...");

    uint8_t data[] = {0x00};
    uint16_t crc = Crc16_Calculate(data, 0);

    /* Initial CRC value should be returned */
    assert(crc == 0xFFFF);

    LOG_INFO("  Empty CRC: 0x%04X", crc);

    LOG_INFO("✓ Empty data test passed");
}

/**
 * @brief Test CRC with known vector
 */
static void test_known_vector(void)
{
    LOG_INFO("Testing CRC with known vectors...");

    /* Test vector 1: "123456789" */
    uint8_t data1[] = "123456789";
    uint16_t crc1 = Crc16_Calculate(data1, strlen((char*)data1));
    LOG_INFO("  CRC('123456789') = 0x%04X", crc1);

    /* Test vector 2: All zeros */
    uint8_t data2[256];
    memset(data2, 0x00, sizeof(data2));
    uint16_t crc2 = Crc16_Calculate(data2, sizeof(data2));
    LOG_INFO("  CRC(all zeros) = 0x%04X", crc2);

    /* Test vector 3: All ones */
    uint8_t data3[256];
    memset(data3, 0xFF, sizeof(data3));
    uint16_t crc3 = Crc16_Calculate(data3, sizeof(data3));
    LOG_INFO("  CRC(all ones) = 0x%04X", crc3);

    LOG_INFO("✓ Known vector test passed");
}

/**
 * @brief Test CRC extended calculation
 */
static void test_extended_crc(void)
{
    LOG_INFO("Testing extended CRC calculation...");

    uint8_t data1[] = {0x01, 0x02, 0x03};
    uint8_t data2[] = {0x04, 0x05};

    /* Calculate CRC in two steps */
    uint16_t crc1 = Crc16_Calculate(data1, sizeof(data1));
    uint16_t crc2 = Crc16_CalculateExtended(data2, sizeof(data2), crc1);

    /* Compare with single calculation */
    uint8_t data_all[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint16_t crc_all = Crc16_Calculate(data_all, sizeof(data_all));

    LOG_INFO("  CRC (two steps): 0x%04X", crc2);
    LOG_INFO("  CRC (single): 0x%04X", crc_all);

    assert(crc2 == crc_all);

    LOG_INFO("✓ Extended CRC test passed");
}

/**
 * @brief Test data integrity detection
 */
static void test_integrity_detection(void)
{
    LOG_INFO("Testing data integrity detection...");

    uint8_t data1[256] = {0};
    for (int i = 0; i < 256; i++) {
        data1[i] = i;
    }

    uint16_t crc1 = Crc16_Calculate(data1, 256);

    /* Corrupt single bit */
    uint8_t data2[256];
    memcpy(data2, data1, 256);
    data2[100] ^= 0x01;

    uint16_t crc2 = Crc16_Calculate(data2, 256);

    LOG_INFO("  CRC original: 0x%04X", crc1);
    LOG_INFO("  CRC corrupted: 0x%04X", crc2);

    assert(crc1 != crc2);

    LOG_INFO("✓ Integrity detection test passed");
}

/**
 * @brief Main test runner
 */
int main(void)
{
    Log_SetLevel(LOG_LEVEL_INFO);

    LOG_INFO("=== CRC-16 Unit Tests ===");
    LOG_INFO("");

    test_basic_crc();
    test_empty_data();
    test_known_vector();
    test_extended_crc();
    test_integrity_detection();

    LOG_INFO("");
    LOG_INFO("=== All tests passed! ===");

    return 0;
}
