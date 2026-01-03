/**
 * @file test_eeprom_driver.c
 * @brief Unit tests for EEPROM driver
 *
 * REQ-EEPROM物理参数模型: design/01-EEPROM基础知识.md §1
 * - 测试页/块对齐验证
 * - 测试读/写/擦除操作
 * - 测试延时模拟
 * - 测试寿命跟踪
 */

#include "eeprom_driver.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/**
 * @brief Test EEPROM initialization
 */
static void test_init(void)
{
    LOG_INFO("Testing EEPROM initialization...");

    Std_ReturnType ret = Eep_Init(NULL);
    assert(ret == E_OK);

    const Eeprom_ConfigType *config = Eep_GetConfig();
    assert(config != NULL);
    assert(config->capacity_bytes == 4096);
    assert(config->page_size == 256);
    assert(config->block_size == 1024);

    Eep_Destroy();

    LOG_INFO("✓ Initialization test passed");
}

/**
 * @brief Test page alignment
 */
static void test_page_alignment(void)
{
    LOG_INFO("Testing page alignment...");

    Eep_Init(NULL);

    assert(Eep_IsPageAligned(0) == TRUE);
    assert(Eep_IsPageAligned(256) == TRUE);
    assert(Eep_IsPageAligned(512) == TRUE);
    assert(Eep_IsPageAligned(128) == FALSE);
    assert(Eep_IsPageAligned(100) == FALSE);

    Eep_Destroy();

    LOG_INFO("✓ Page alignment test passed");
}

/**
 * @brief Test block alignment
 */
static void test_block_alignment(void)
{
    LOG_INFO("Testing block alignment...");

    Eep_Init(NULL);

    assert(Eep_IsBlockAligned(0) == TRUE);
    assert(Eep_IsBlockAligned(1024) == TRUE);
    assert(Eep_IsBlockAligned(2048) == TRUE);
    assert(Eep_IsBlockAligned(512) == FALSE);
    assert(Eep_IsBlockAligned(100) == FALSE);

    Eep_Destroy();

    LOG_INFO("✓ Block alignment test passed");
}

/**
 * @brief Test write operation
 */
static void test_write(void)
{
    LOG_INFO("Testing write operation...");

    Eep_Init(NULL);

    uint8_t data[256];
    memset(data, 0xAA, 256);

    Std_ReturnType ret = Eep_Write(0, data, 256);
    assert(ret == E_OK);

    /* Test non-page-aligned write (should fail) */
    ret = Eep_Write(128, data, 256);
    assert(ret == E_NOT_OK);

    /* Test write to non-empty page (should fail) */
    ret = Eep_Write(0, data, 256);
    assert(ret == E_NOT_OK);

    Eep_Destroy();

    LOG_INFO("✓ Write test passed");
}

/**
 * @brief Test read operation
 */
static void test_read(void)
{
    LOG_INFO("Testing read operation...");

    Eep_Init(NULL);

    uint8_t write_data[256];
    memset(write_data, 0x55, 256);

    /* Write data */
    Eep_Erase(0);
    Eep_Write(0, write_data, 256);

    /* Read back */
    uint8_t read_data[256];
    memset(read_data, 0x00, 256);

    Std_ReturnType ret = Eep_Read(0, read_data, 256);
    assert(ret == E_OK);
    assert(memcmp(write_data, read_data, 256) == 0);

    Eep_Destroy();

    LOG_INFO("✓ Read test passed");
}

/**
 * @brief Test erase operation
 */
static void test_erase(void)
{
    LOG_INFO("Testing erase operation...");

    Eep_Init(NULL);

    uint8_t data[256];
    memset(data, 0xAA, 256);

    /* Write data */
    Eep_Write(0, data, 256);

    /* Erase block */
    Std_ReturnType ret = Eep_Erase(0);
    assert(ret == E_OK);

    /* Read back and verify erased (all 0xFF) */
    uint8_t read_data[256];
    Eep_Read(0, read_data, 256);

    for (int i = 0; i < 256; i++) {
        assert(read_data[i] == 0xFF);
    }

    Eep_Destroy();

    LOG_INFO("✓ Erase test passed");
}

/**
 * @brief Test diagnostics
 */
static void test_diagnostics(void)
{
    LOG_INFO("Testing diagnostics...");

    Eep_Init(NULL);

    uint8_t data[256];
    memset(data, 0xAA, 256);

    Eep_Erase(0);
    Eep_Write(0, data, 256);
    Eep_Read(0, data, 256);

    Eeprom_DiagInfoType diag;
    Std_ReturnType ret = Eep_GetDiagnostics(&diag);
    assert(ret == E_OK);

    assert(diag.total_read_count > 0);
    assert(diag.total_write_count > 0);
    assert(diag.total_erase_count > 0);

    LOG_INFO("  Read count: %u", diag.total_read_count);
    LOG_INFO("  Write count: %u", diag.total_write_count);
    LOG_INFO("  Erase count: %u", diag.total_erase_count);

    Eep_Destroy();

    LOG_INFO("✓ Diagnostics test passed");
}

/**
 * @brief Test endurance tracking
 */
static void test_endurance(void)
{
    LOG_INFO("Testing endurance tracking...");

    Eep_Init(NULL);

    Eeprom_DiagInfoType diag;

    /* Erase block multiple times */
    for (int i = 0; i < 10; i++) {
        Eep_Erase(0);
    }

    Eep_GetDiagnostics(&diag);
    assert(diag.total_erase_count == 10);
    assert(diag.max_erase_count == 10);

    LOG_INFO("  Erase count after 10 erases: %u", diag.max_erase_count);

    Eep_Destroy();

    LOG_INFO("✓ Endurance test passed");
}

/**
 * @brief Main test runner
 */
int main(void)
{
    Log_SetLevel(LOG_LEVEL_INFO);

    LOG_INFO("=== EEPROM Driver Unit Tests ===");
    LOG_INFO("");

    test_init();
    test_page_alignment();
    test_block_alignment();
    test_write();
    test_read();
    test_erase();
    test_diagnostics();
    test_endurance();

    LOG_INFO("");
    LOG_INFO("=== All tests passed! ===");

    return 0;
}
