/**
 * @file fault_injection.h
 * @brief Fault Injection Framework for EEPROM Simulation
 *
 * REQ-故障注入: design/04-数据完整性方案.md §5
 * REQ-故障库: design/07-系统测试与故障场景.md §2
 * - P0级故障: F01-F05 (PowerLoss, Timeout, BitFlip, CRC, Verify)
 * - Hook机制: Eep_Read/Eep_Write/Crc_Calculate
 * - 可配置的故障概率和触发条件
 */

#ifndef FAULT_INJECTION_H
#define FAULT_INJECTION_H

#include "common_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fault IDs (P0-01 to P0-10)
 */
typedef enum {
    FAULT_NONE = 0,

    /* P0-01: Power loss during page program */
    FAULT_P0_POWERLOSS_PAGEPROGRAM = 0x01,

    /* P0-02: Power loss during WriteAll phase 2 */
    FAULT_P0_POWERLOSS_WRITEALL = 0x02,

    /* P0-03: Single bit flip after read */
    FAULT_P0_BITFLIP_SINGLE = 0x03,

    /* P0-04: Multiple bit flip after read */
    FAULT_P0_BITFLIP_MULTI = 0x04,

    /* P0-05: NvM_MainFunction timeout (50ms) */
    FAULT_P0_TIMEOUT_50MS = 0x05,

    /* P0-06: Erase operation timeout */
    FAULT_P0_TIMEOUT_ERASE = 0x06,

    /* P0-07: CRC calculation inversion */
    FAULT_P0_CRC_INVERT = 0x07,

    /* P0-08: Write verify always fail */
    FAULT_P0_WRITE_VERIFY_FAIL = 0x08,

    /* P0-09: RAM corruption before write */
    FAULT_P0_RAM_CORRUPT = 0x09,

    /* P0-10: Job queue overflow */
    FAULT_P0_QUEUE_OVERFLOW = 0x0A,

    FAULT_MAX_ID = 0xFF
} FaultId_t;

/**
 * @brief Fault configuration
 */
typedef struct {
    FaultId_t fault_id;
    boolean enabled;
    uint8_t target_block_id;      /**< 0xFF = all blocks */
    uint16_t trigger_count;       /**< 0 = trigger every time */
    uint16_t triggered_count;     /**< Internal counter */
    uint8_t probability_percent;  /**< 0-100, 0 = always, 100 = never */
} FaultConfig_t;

/**
 * @brief Fault injection statistics
 */
typedef struct {
    uint32_t total_triggered;
    uint32_t total_injected;
    uint32_t injection_failures;
} FaultStats_t;

/**
 * @brief Initialize fault injection framework
 */
void FaultInj_Init(void);

/**
 * @brief Enable a specific fault
 *
 * @param fault_id Fault ID
 * @return E_OK on success
 */
Std_ReturnType FaultInj_Enable(FaultId_t fault_id);

/**
 * @brief Disable a specific fault
 *
 * @param fault_id Fault ID
 * @return E_OK on success
 */
Std_ReturnType FaultInj_Disable(FaultId_t fault_id);

/**
 * @brief Check if a fault is enabled
 *
 * @param fault_id Fault ID
 * @return TRUE if enabled
 */
boolean FaultInj_IsEnabled(FaultId_t fault_id);

/**
 * @brief Configure fault parameters
 *
 * @param config Fault configuration
 * @return E_OK on success
 */
Std_ReturnType FaultInj_Configure(const FaultConfig_t *config);

/**
 * @brief Get fault statistics
 *
 * @param stats Pointer to statistics structure
 * @return E_OK on success
 */
Std_ReturnType FaultInj_GetStats(FaultStats_t *stats);

/**
 * @brief Reset fault statistics
 */
void FaultInj_ResetStats(void);

/**
 * @brief Reset all fault configurations
 */
void FaultInj_ResetAll(void);

/**
 * @brief Hook: Called before EEPROM read
 *
 * @param address EEPROM address
 * @param length Data length
 * @return TRUE if fault should be injected
 */
boolean FaultInj_HookBeforeRead(uint32_t address, uint32_t length);

/**
 * @brief Hook: Called after EEPROM read
 *
 * @param data Data buffer (can be modified to inject bit flip)
 * @param length Data length
 * @return TRUE if fault was injected
 */
boolean FaultInj_HookAfterRead(uint8_t *data, uint32_t length);

/**
 * @brief Hook: Called before EEPROM write
 *
 * @param address EEPROM address
 * @param length Data length
 * @return TRUE if write should be blocked
 */
boolean FaultInj_HookBeforeWrite(uint32_t address, uint32_t length);

/**
 * @brief Hook: Called after EEPROM write
 *
 * @param address EEPROM address
 * @return TRUE if power loss should be simulated
 */
boolean FaultInj_HookAfterWrite(uint32_t address);

/**
 * @brief Hook: Called for CRC calculation
 *
 * @param data Input data
 * @param length Data length
 * @param crc Calculated CRC (can be modified)
 * @return TRUE if CRC was modified
 */
boolean FaultInj_HookCrc(const uint8_t *data, uint32_t length, uint16_t *crc);

/**
 * @brief Hook: Called for write verification
 *
 * @param address EEPROM address
 * @param expected_data Expected data
 * @param actual_data Actual read-back data (can be modified)
 * @param length Data length
 * @return TRUE if verification should fail
 */
boolean FaultInj_HookVerify(uint32_t address, const uint8_t *expected_data,
                            uint8_t *actual_data, uint32_t length);

/**
 * @brief Hook: Called before RAM mirror read
 *
 * @param block_id Block ID
 * @param data RAM data (can be modified)
 * @param length Data length
 * @return TRUE if corruption was injected
 */
boolean FaultInj_HookRamMirror(uint8_t block_id, uint8_t *data, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* FAULT_INJECTION_H */
