/**
 * @file eeprom_driver.c
 * @brief EEPROM hardware simulation driver implementation
 *
 * REQ-EEPROM物理参数模型: design/01-EEPROM基础知识.md §1
 * - 容量、页大小、块大小参数化
 * - 读/写/擦除延时模拟
 * - 寿命计数与跟踪
 */

#include "eeprom_driver.h"
#include "fault_injection.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief Default EEPROM configuration
 *
 * Based on automotive industry typical values
 */
static const Eeprom_ConfigType default_config = {
    .capacity_bytes = 4096,      /**< 4KB capacity */
    .page_size = 256,            /**< 256B pages */
    .block_size = 1024,          /**< 1KB blocks (4 blocks total) */
    .read_delay_us = 50,         /**< 50µs per byte */
    .write_delay_ms = 2,         /**< 2ms per page */
    .erase_delay_ms = 3,         /**< 3ms per block */
    .endurance_cycles = 100000,  /**< 100K cycles endurance */
    .virtual_storage = NULL
};

/**
 * @brief Global configuration pointer
 */
static Eeprom_ConfigType g_config = {0};

/**
 * @brief Per-block erase count tracking
 */
static uint32_t *g_erase_counts = NULL;

/**
 * @brief Diagnostic counters
 */
static Eeprom_DiagInfoType g_diagnostics = {0};

/**
 * @brief Time scale factor for simulation speed control
 */
static uint32_t g_time_scale = 1;

/**
 * @brief Initialization flag
 */
static boolean g_initialized = FALSE;

/**
 * @brief Simulate delay (affected by time scale)
 *
 * @param delay_us Delay in microseconds (real-time)
 */
static void simulate_delay_us(uint32_t delay_us)
{
    /* In real simulation, this would use the virtual OS scheduler
     * For now, just a placeholder */
    (void)delay_us;
    (void)g_time_scale;
}

/**
 * @brief Simulate delay in milliseconds
 *
 * @param delay_ms Delay in milliseconds (real-time)
 */
static void simulate_delay_ms(uint32_t delay_ms)
{
    /* Apply time scale */
    uint32_t scaled_delay = delay_ms / g_time_scale;
    (void)scaled_delay;
    /* Actual delay would be implemented by virtual OS scheduler */
}

/**
 * @brief Validate address range
 *
 * @param address Address to validate
 * @param length Access length
 * @return TRUE if valid, FALSE otherwise
 */
static boolean validate_address(uint32_t address, uint32_t length)
{
    if (!g_initialized) {
        return FALSE;
    }

    if (address >= g_config.capacity_bytes) {
        return FALSE;
    }

    if ((address + length) > g_config.capacity_bytes) {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Calculate block index from address
 *
 * @param address Byte address
 * @return Block index
 */
static uint32_t address_to_block(uint32_t address)
{
    return address / g_config.block_size;
}

Std_ReturnType Eep_Init(const Eeprom_ConfigType *config)
{
    if (config == NULL) {
        /* Use default configuration */
        g_config = default_config;
    } else {
        /* Use provided configuration */
        g_config = *config;
    }

    /* Allocate virtual storage */
    if (g_config.virtual_storage == NULL) {
        g_config.virtual_storage = (uint8_t *)calloc(g_config.capacity_bytes, sizeof(uint8_t));
        if (g_config.virtual_storage == NULL) {
            return E_NOT_OK;
        }

        /* Initialize with 0xFF (erased state) */
        memset(g_config.virtual_storage, 0xFF, g_config.capacity_bytes);
    }

    /* Allocate erase count tracking */
    uint32_t num_blocks = g_config.capacity_bytes / g_config.block_size;
    g_erase_counts = (uint32_t *)calloc(num_blocks, sizeof(uint32_t));
    if (g_erase_counts == NULL) {
        free(g_config.virtual_storage);
        g_config.virtual_storage = NULL;
        return E_NOT_OK;
    }

    /* Reset diagnostics */
    memset(&g_diagnostics, 0, sizeof(Eeprom_DiagInfoType));

    g_initialized = TRUE;
    return E_OK;
}

Std_ReturnType Eep_Read(uint32_t address, uint8_t *data_buffer, uint32_t length)
{
    /* Validate parameters */
    if (data_buffer == NULL) {
        return E_NOT_OK;
    }

    if (!validate_address(address, length)) {
        return E_NOT_OK;
    }

    /* Fault injection hook: Before read */
    if (FaultInj_HookBeforeRead(address, length)) {
        /* Hook indicates read should be blocked */
        return E_NOT_OK;
    }

    /* Simulate read delay */
    uint32_t total_delay_us = length * g_config.read_delay_us;
    simulate_delay_us(total_delay_us);

    /* Read data from virtual storage */
    memcpy(data_buffer, &g_config.virtual_storage[address], length);

    /* Fault injection hook: After read (bit flip) */
    FaultInj_HookAfterRead(data_buffer, length);

    /* Update diagnostics */
    g_diagnostics.total_read_count++;
    g_diagnostics.total_bytes_read += length;

    return E_OK;
}

Std_ReturnType Eep_Write(uint32_t address, const uint8_t *data_buffer, uint32_t length)
{
    /* Validate parameters */
    if (data_buffer == NULL) {
        return E_NOT_OK;
    }

    if (!validate_address(address, length)) {
        return E_NOT_OK;
    }

    /* Fault injection hook: Before write (e.g., erase timeout) */
    if (FaultInj_HookBeforeWrite(address, length)) {
        /* Hook indicates write should be blocked */
        return E_NOT_OK;
    }

    /* Check page alignment */
    if (!Eep_IsPageAligned(address)) {
        return E_NOT_OK;
    }

    if ((length % g_config.page_size) != 0) {
        return E_NOT_OK;
    }

    /* Check if target pages are empty (0xFF) */
    for (uint32_t i = 0; i < length; i++) {
        if (g_config.virtual_storage[address + i] != 0xFF) {
            /* Page not empty, need erase first */
            return E_NOT_OK;
        }
    }

    /* Calculate number of pages */
    uint32_t num_pages = length / g_config.page_size;

    /* Simulate write delay */
    uint32_t total_delay_ms = num_pages * g_config.write_delay_ms;
    simulate_delay_ms(total_delay_ms);

    /* Write data to virtual storage */
    memcpy(&g_config.virtual_storage[address], data_buffer, length);

    /* Update diagnostics */
    g_diagnostics.total_write_count++;
    g_diagnostics.total_bytes_written += length;

    /* Fault injection hook: After write (e.g., power loss) */
    if (FaultInj_HookAfterWrite(address)) {
        /* Power loss simulated - data written but may be inconsistent */
        return E_NOT_OK;
    }

    return E_OK;
}

Std_ReturnType Eep_Erase(uint32_t address)
{
    /* Validate parameters */
    if (!validate_address(address, g_config.block_size)) {
        return E_NOT_OK;
    }

    /* Check block alignment */
    if (!Eep_IsBlockAligned(address)) {
        return E_NOT_OK;
    }

    /* Get block index */
    uint32_t block_idx = address_to_block(address);

    /* Check endurance */
    if (g_erase_counts[block_idx] >= g_config.endurance_cycles) {
        /* Endurance exceeded */
        return E_NOT_OK;
    }

    /* Simulate erase delay */
    simulate_delay_ms(g_config.erase_delay_ms);

    /* Erase block (set to 0xFF) */
    memset(&g_config.virtual_storage[address], 0xFF, g_config.block_size);

    /* Update erase count */
    g_erase_counts[block_idx]++;
    g_diagnostics.total_erase_count++;

    /* Update max erase count */
    if (g_erase_counts[block_idx] > g_diagnostics.max_erase_count) {
        g_diagnostics.max_erase_count = g_erase_counts[block_idx];
    }

    return E_OK;
}

Std_ReturnType Eep_GetDiagnostics(Eeprom_DiagInfoType *diag_info)
{
    if (diag_info == NULL) {
        return E_NOT_OK;
    }

    if (!g_initialized) {
        return E_NOT_OK;
    }

    *diag_info = g_diagnostics;
    return E_OK;
}

boolean Eep_IsPageAligned(uint32_t address)
{
    if (!g_initialized) {
        return FALSE;
    }

    return (address % g_config.page_size) == 0;
}

boolean Eep_IsBlockAligned(uint32_t address)
{
    if (!g_initialized) {
        return FALSE;
    }

    return (address % g_config.block_size) == 0;
}

const Eeprom_ConfigType* Eep_GetConfig(void)
{
    if (!g_initialized) {
        return NULL;
    }

    return &g_config;
}

Std_ReturnType Eep_SetTimeScale(uint32_t scale)
{
    if (scale == 0) {
        return E_NOT_OK;
    }

    g_time_scale = scale;
    return E_OK;
}

void Eep_Destroy(void)
{
    if (g_config.virtual_storage != NULL) {
        free(g_config.virtual_storage);
        g_config.virtual_storage = NULL;
    }

    if (g_erase_counts != NULL) {
        free(g_erase_counts);
        g_erase_counts = NULL;
    }

    g_initialized = FALSE;
}
