/**
 * @file eeprom_basics.c
 * @brief Basic EEPROM operations example
 *
 * REQ-存储介质基础知识: design/01-EEPROM基础知识.md
 * - 展示页对齐写操作
 * - 展示块擦除操作
 * - 展示读写操作
 * - 展示诊断信息获取
 */

#include "eeprom_driver.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Demonstrate page-aligned write
 */
static void demo_page_aligned_write(void)
{
    LOG_INFO("=== Demo: Page-Aligned Write ===");

    /* Initialize EEPROM */
    Eep_Init(NULL);

    const Eeprom_ConfigType *config = Eep_GetConfig();
    LOG_INFO("EEPROM Capacity: %u bytes", config->capacity_bytes);
    LOG_INFO("Page Size: %u bytes", config->page_size);
    LOG_INFO("Block Size: %u bytes", config->block_size);

    /* Prepare data */
    uint8_t data[256];
    for (int i = 0; i < 256; i++) {
        data[i] = i;
    }

    /* Erase block first (required before write) */
    LOG_INFO("Erasing block at 0x0000...");
    Std_ReturnType ret = Eep_Erase(0);
    if (ret == E_OK) {
        LOG_INFO("✓ Block erased successfully");
    } else {
        LOG_ERROR("✗ Block erase failed");
        return;
    }

    /* Write data (page-aligned) */
    LOG_INFO("Writing 256 bytes at address 0x0100 (page-aligned)...");
    ret = Eep_Write(0x0100, data, 256);
    if (ret == E_OK) {
        LOG_INFO("✓ Write successful");
    } else {
        LOG_ERROR("✗ Write failed");
        return;
    }

    /* Try non-aligned write (should fail) */
    LOG_INFO("Attempting non-aligned write at 0x0100 (should fail)...");
    ret = Eep_Write(0x0100, data, 128);
    if (ret == E_NOT_OK) {
        LOG_INFO("✓ Non-aligned write correctly rejected");
    }

    Eep_Destroy();
}

/**
 * @brief Demonstrate read operation
 */
static void demo_read_operation(void)
{
    LOG_INFO("");
    LOG_INFO("=== Demo: Read Operation ===");

    Eep_Init(NULL);

    /* Write test data */
    uint8_t write_data[256] = {0xAA, 0xBB, 0xCC, 0xDD};
    Eep_Erase(0);
    Eep_Write(0x0200, write_data, 256);

    /* Read back */
    uint8_t read_data[256];
    memset(read_data, 0, sizeof(read_data));

    LOG_INFO("Reading 256 bytes from address 0x0200...");
    Std_ReturnType ret = Eep_Read(0x0200, read_data, 256);
    if (ret == E_OK) {
        LOG_INFO("✓ Read successful");
        LOG_INFO("  First 4 bytes: %02X %02X %02X %02X",
                 read_data[0], read_data[1], read_data[2], read_data[3]);

        /* Verify data */
        if (memcmp(write_data, read_data, 256) == 0) {
            LOG_INFO("✓ Data verification passed");
        } else {
            LOG_ERROR("✗ Data verification failed");
        }
    }

    Eep_Destroy();
}

/**
 * @brief Demonstrate diagnostics
 */
static void demo_diagnostics(void)
{
    LOG_INFO("");
    LOG_INFO("=== Demo: Diagnostics ===");

    Eep_Init(NULL);

    /* Perform some operations */
    uint8_t data[256];
    memset(data, 0x55, 256);

    for (int i = 0; i < 5; i++) {
        Eep_Erase(0);
        Eep_Write(0, data, 256);
        Eep_Read(0, data, 256);
    }

    /* Get diagnostics */
    Eeprom_DiagInfoType diag;
    Std_ReturnType ret = Eep_GetDiagnostics(&diag);
    if (ret == E_OK) {
        LOG_INFO("✓ Diagnostics retrieved:");
        LOG_INFO("  Total reads: %u", diag.total_read_count);
        LOG_INFO("  Total writes: %u", diag.total_write_count);
        LOG_INFO("  Total erases: %u", diag.total_erase_count);
        LOG_INFO("  Max erase count: %u", diag.max_erase_count);
        LOG_INFO("  Bytes read: %u", diag.total_bytes_read);
        LOG_INFO("  Bytes written: %u", diag.total_bytes_written);
    }

    Eep_Destroy();
}

/**
 * @brief Demonstrate write amplification
 */
static void demo_write_amplification(void)
{
    LOG_INFO("");
    LOG_INFO("=== Demo: Write Amplification ===");

    Eep_Init(NULL);

    const Eeprom_ConfigType *config = Eep_GetConfig();
    LOG_INFO("Block Size: %u bytes", config->block_size);
    LOG_INFO("Page Size: %u bytes", config->page_size);

    /* Scenario: Write 32 bytes of data */
    uint32_t data_size = 32;
    LOG_INFO("Data size to write: %u bytes", data_size);

    /* Calculate actual write size */
    uint32_t pages_to_write = (data_size + config->page_size - 1) / config->page_size;
    uint32_t actual_write_size = pages_to_write * config->page_size;

    LOG_INFO("Pages required: %u", pages_to_write);
    LOG_INFO("Actual write size: %u bytes", actual_write_size);
    LOG_INFO("Write amplification factor: %.2fx",
             (float)actual_write_size / data_size);

    /* Explanation */
    LOG_INFO("");
    LOG_INFO("Note: Even for 32 bytes, we must write a full page (256 bytes)");
    LOG_INFO("This demonstrates the write amplification inherent in EEPROM");

    Eep_Destroy();
}

/**
 * @brief Main function
 */
int main(void)
{
    Log_SetLevel(LOG_LEVEL_INFO);

    LOG_INFO("========================================");
    LOG_INFO("  EEPROM Basics Example");
    LOG_INFO("========================================");
    LOG_INFO("");

    demo_page_aligned_write();
    demo_read_operation();
    demo_diagnostics();
    demo_write_amplification();

    LOG_INFO("");
    LOG_INFO("========================================");
    LOG_INFO("  Example completed successfully!");
    LOG_INFO("========================================");

    return 0;
}
