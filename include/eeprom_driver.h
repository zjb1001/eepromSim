/**
 * @file eeprom_driver.h
 * @brief EEPROM hardware simulation driver interface
 *
 * REQ-EEPROM物理参数模型: design/01-EEPROM基础知识.md §1
 * - 参数化配置: 容量、页大小、块大小、延时
 * - 寿命跟踪: 擦写计数、磨损均衡
 * - 延时模拟: 读/写/擦除操作延时
 */

#ifndef EEPROM_DRIVER_H
#define EEPROM_DRIVER_H

#include "common_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief EEPROM configuration parameters
 *
 * Default values based on automotive industry typicals:
 * - 4KB capacity, 256B pages, 16KB blocks
 * - 50µs read, 2ms write, 3ms erase delays
 * - 100K cycles endurance
 */
typedef struct {
    uint32_t capacity_bytes;       /**< Total capacity in bytes */
    uint32_t page_size;            /**< Page size in bytes (write alignment) */
    uint32_t block_size;           /**< Block size in bytes (erase alignment) */
    uint32_t read_delay_us;        /**< Read delay per byte (microseconds) */
    uint32_t write_delay_ms;       /**< Write delay per page (milliseconds) */
    uint32_t erase_delay_ms;       /**< Erase delay per block (milliseconds) */
    uint32_t endurance_cycles;     /**< Endurance in erase/write cycles */
    uint8_t  *virtual_storage;     /**< Simulated EEPROM storage */
} Eeprom_ConfigType;

/**
 * @brief EEPROM diagnostic information
 */
typedef struct {
    uint32_t total_read_count;     /**< Total read operations */
    uint32_t total_write_count;    /**< Total write operations */
    uint32_t total_erase_count;    /**< Total erase operations */
    uint32_t max_erase_count;      /**< Maximum erase count for any block */
    uint32_t crc_error_count;      /**< CRC error count */
    uint32_t total_bytes_read;     /**< Total bytes read */
    uint32_t total_bytes_written;  /**< Total bytes written */
} Eeprom_DiagInfoType;

/**
 * @brief Initialize EEPROM driver
 *
 * @param config Pointer to configuration structure
 * @return E_OK on success, E_NOT_OK on failure
 *
 * REQ-初始化: 必须在调用其他API前调用一次
 */
Std_ReturnType Eep_Init(const Eeprom_ConfigType *config);

/**
 * @brief Read data from EEPROM
 *
 * @param address Byte offset to read from
 * @param data_buffer Buffer to store read data
 * @param length Number of bytes to read
 * @return E_OK on success, E_NOT_OK on failure
 *
 * REQ-读操作: 不受页/块限制，任意粒度读取
 * 延时: length × read_delay_us
 */
Std_ReturnType Eep_Read(uint32_t address, uint8_t *data_buffer, uint32_t length);

/**
 * @brief Write data to EEPROM
 *
 * @param address Byte offset to write to (must be page-aligned)
 * @param data_buffer Data to write
 * @param length Number of bytes to write (must be multiple of page size)
 * @return E_OK on success, E_NOT_OK on failure
 *
 * REQ-写操作: 必须页对齐，目标页必须为空(0xFF)
 * 延时: (length / page_size) × write_delay_ms
 */
Std_ReturnType Eep_Write(uint32_t address, const uint8_t *data_buffer, uint32_t length);

/**
 * @brief Erase a block in EEPROM
 *
 * @param address Block start address (must be block-aligned)
 * @return E_OK on success, E_NOT_OK on failure
 *
 * REQ-擦除操作: 必须块对齐，擦除后整块变为0xFF
 * 延时: erase_delay_ms
 */
Std_ReturnType Eep_Erase(uint32_t address);

/**
 * @brief Get diagnostic information
 *
 * @param diag_info Pointer to diagnostic info structure
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType Eep_GetDiagnostics(Eeprom_DiagInfoType *diag_info);

/**
 * @brief Check if address is page-aligned
 *
 * @param address Address to check
 * @return TRUE if aligned, FALSE otherwise
 */
boolean Eep_IsPageAligned(uint32_t address);

/**
 * @brief Check if address is block-aligned
 *
 * @param address Address to check
 * @return TRUE if aligned, FALSE otherwise
 */
boolean Eep_IsBlockAligned(uint32_t address);

/**
 * @brief Get EEPROM configuration
 *
 * @return Pointer to current configuration or NULL if not initialized
 */
const Eeprom_ConfigType* Eep_GetConfig(void);

/**
 * @brief Set time scale for simulation (for testing)
 *
 * @param scale Time scale factor (1=real-time, 10=10x faster, etc.)
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType Eep_SetTimeScale(uint32_t scale);

/**
 * @brief Destroy EEPROM driver and cleanup resources
 */
void Eep_Destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* EEPROM_DRIVER_H */
