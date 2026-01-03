/**
 * @file eeprom_layout.h
 * @brief EEPROM Layout and Address Calculation
 *
 * Defines the physical layout of blocks in EEPROM memory,
 * including data, CRC, and reserved regions.
 */

#ifndef EEPROM_LAYOUT_H
#define EEPROM_LAYOUT_H

#include <stdint.h>
#include <stddef.h>
#include "common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief EEPROM Block Slot Size
 *
 * Each block occupies a fixed-size slot (1024 bytes) aligned to slot boundaries.
 * This ensures CRC and metadata don't conflict with adjacent blocks.
 */
#define EEPROM_BLOCK_SLOT_SIZE  1024

/**
 * @brief Maximum number of blocks for 4KB EEPROM
 */
#define EEPROM_MAX_BLOCKS_4KB   4

/**
 * @brief Maximum number of blocks for 8KB EEPROM
 */
#define EEPROM_MAX_BLOCKS_8KB   8

/**
 * @brief Calculate CRC offset for a block
 *
 * @param block Block configuration
 * @return CRC offset in EEPROM
 */
#define EEPROM_CRC_OFFSET(block) ((block)->eeprom_offset + (block)->block_size)

/**
 * @brief Check if offset is slot-aligned
 *
 * @param offset EEPROM offset
 * @return TRUE if aligned to 1024-byte boundary
 */
#define EEPROM_IS_SLOT_ALIGNED(offset) (((offset) % EEPROM_BLOCK_SLOT_SIZE) == 0)

/**
 * @brief Calculate next block slot offset
 *
 * @param current_offset Current block offset
 * @return Next block slot offset
 */
#define EEPROM_NEXT_SLOT_OFFSET(current_offset) ((current_offset) + EEPROM_BLOCK_SLOT_SIZE)

/**
 * @brief EEPROM Block Layout Structure
 */
typedef struct {
    uint32_t data_offset;      /**< Data region offset (slot start) */
    uint32_t data_size;        /**< Data size in bytes */
    uint32_t crc_offset;       /**< CRC offset (data_offset + data_size) */
    uint32_t crc_size;         /**< CRC size (typically 2 for CRC16) */
    uint32_t reserved_start;   /**< Reserved region start */
    uint32_t reserved_size;    /**< Reserved region size */
    uint32_t slot_size;        /**< Total slot size */
} Eeprom_BlockLayout_t;

/**
 * @brief Calculate block layout
 *
 * @param block Block configuration
 * @param layout Pointer to store calculated layout
 * @return 0 on success, -1 on error
 */
int EEPROM_CalcBlockLayout(const void *block, Eeprom_BlockLayout_t *layout);

/**
 * @brief Validate block configuration
 *
 * @param block Block configuration
 * @return TRUE if configuration is valid
 */
boolean EEPROM_ValidateBlockConfig(const void *block);

/**
 * @brief Calculate Dataset block version offset
 *
 * @param base_offset Base offset for dataset block
 * @param dataset_index Dataset version index (0-based)
 * @return Offset for the specified dataset version
 */
uint32_t EEPROM_DatasetVersionOffset(uint32_t base_offset, uint8_t dataset_index);

#ifdef __cplusplus
}
#endif

#endif /* EEPROM_LAYOUT_H */
