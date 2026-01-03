/**
 * @file nvm_block_types.h
 * @brief NvM Block Type Implementations
 *
 * Internal functions for Native, Redundant, and Dataset block handling
 */

#ifndef NVM_BLOCK_TYPES_H
#define NVM_BLOCK_TYPES_H

#include "nvm.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Try to read block with CRC verification
 *
 * @param offset EEPROM offset
 * @param data Data buffer
 * @param size Block size
 * @param crc_type CRC type
 * @return TRUE if successful
 */
boolean NvM_TryReadBlock(uint32_t offset, uint8_t *data, uint16_t size, NvM_CrcType_t crc_type);

/**
 * @brief Write block with CRC
 *
 * @param offset EEPROM offset
 * @param data Data buffer
 * @param size Block size
 * @param crc_type CRC type
 * @return E_OK if successful
 */
Std_ReturnType NvM_WriteBlockWithCrc(uint32_t offset, const uint8_t *data,
                                     uint16_t size, NvM_CrcType_t crc_type);

/**
 * @brief Read Native Block
 *
 * @param block Block configuration
 * @param data Data buffer
 * @return E_OK if successful
 */
Std_ReturnType NvM_ReadNativeBlock(NvM_BlockConfig_t *block, void *data);

/**
 * @brief Write Native Block
 *
 * @param block Block configuration
 * @param data Data buffer
 * @return E_OK if successful
 */
Std_ReturnType NvM_WriteNativeBlock(NvM_BlockConfig_t *block, const void *data);

/**
 * @brief Read Redundant Block
 *
 * @param block Block configuration
 * @param data Data buffer
 * @return E_OK if successful
 */
Std_ReturnType NvM_ReadRedundantBlock(NvM_BlockConfig_t *block, void *data);

/**
 * @brief Write Redundant Block (write primary, then backup)
 *
 * @param block Block configuration
 * @param data Data buffer
 * @return E_OK if successful
 */
Std_ReturnType NvM_WriteRedundantBlock(NvM_BlockConfig_t *block, const void *data);

/**
 * @brief Read Dataset Block
 *
 * @param block Block configuration
 * @param data Data buffer
 * @return E_OK if successful
 */
Std_ReturnType NvM_ReadDatasetBlock(NvM_BlockConfig_t *block, void *data);

/**
 * @brief Write Dataset Block (round-robin)
 *
 * @param block Block configuration
 * @param data Data buffer
 * @return E_OK if successful
 */
Std_ReturnType NvM_WriteDatasetBlock(NvM_BlockConfig_t *block, const void *data);

#ifdef __cplusplus
}
#endif

#endif /* NVM_BLOCK_TYPES_H */
