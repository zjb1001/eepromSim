/**
 * @file eeprom_layout.c
 * @brief EEPROM Layout and Address Calculation Implementation
 */

#include "nvm.h"
#include "eeprom_layout.h"
#include "logging.h"
#include <string.h>

/**
 * @brief Calculate block layout
 */
int EEPROM_CalcBlockLayout(const void *block, Eeprom_BlockLayout_t *layout)
{
    const NvM_BlockConfig_t *cfg = (const NvM_BlockConfig_t *)block;

    if (block == NULL || layout == NULL) {
        return -1;
    }

    layout->data_offset = cfg->eeprom_offset;
    layout->data_size = cfg->block_size;
    layout->crc_offset = cfg->eeprom_offset + cfg->block_size;

    /* CRC size based on CRC type */
    switch (cfg->crc_type) {
        case NVM_CRC_NONE:
            layout->crc_size = 0;
            break;
        case NVM_CRC8:
            layout->crc_size = 1;
            break;
        case NVM_CRC16:
            layout->crc_size = 2;
            break;
        case NVM_CRC32:
            layout->crc_size = 4;
            break;
        default:
            layout->crc_size = 2;  /* Default CRC16 */
            break;
    }

    layout->reserved_start = layout->crc_offset + layout->crc_size;
    layout->reserved_size = EEPROM_BLOCK_SLOT_SIZE - layout->reserved_size;
    layout->slot_size = EEPROM_BLOCK_SLOT_SIZE;

    return 0;
}

/**
 * @brief Validate block configuration
 */
boolean EEPROM_ValidateBlockConfig(const void *block)
{
    const NvM_BlockConfig_t *cfg = (const NvM_BlockConfig_t *)block;

    if (block == NULL) {
        return FALSE;
    }

    /* Check slot alignment */
    if (!EEPROM_IS_SLOT_ALIGNED(cfg->eeprom_offset)) {
        LOG_ERROR("EEPROM: Block %d offset 0x%X not aligned to %d-byte boundary",
                 cfg->block_id, cfg->eeprom_offset, EEPROM_BLOCK_SLOT_SIZE);
        return FALSE;
    }

    /* Calculate CRC offset */
    uint32_t crc_offset = EEPROM_CRC_OFFSET(cfg);
    uint32_t crc_size = 2;  /* Assume CRC16 for validation */

    /* Check CRC doesn't exceed slot boundary */
    if ((crc_offset + crc_size) > (cfg->eeprom_offset + EEPROM_BLOCK_SLOT_SIZE)) {
        LOG_ERROR("EEPROM: Block %d data(%d) + CRC(%d) exceeds slot boundary at 0x%X",
                 cfg->block_id, cfg->block_size, crc_size,
                 cfg->eeprom_offset + EEPROM_BLOCK_SLOT_SIZE);
        return FALSE;
    }

    /* Check for Native block */
    if (cfg->block_type == NVM_BLOCK_NATIVE) {
        /* Nothing else to validate */
        return TRUE;
    }

    /* Check for Redundant block */
    if (cfg->block_type == NVM_BLOCK_REDUNDANT) {
        if (!EEPROM_IS_SLOT_ALIGNED(cfg->redundant_eeprom_offset)) {
            LOG_ERROR("EEPROM: Redundant Block %d backup offset 0x%X not aligned",
                     cfg->block_id, cfg->redundant_eeprom_offset);
            return FALSE;
        }

        /* Check backup doesn't overlap with primary */
        uint32_t primary_end = cfg->eeprom_offset + EEPROM_BLOCK_SLOT_SIZE;
        if (cfg->redundant_eeprom_offset < primary_end) {
            LOG_ERROR("EEPROM: Redundant Block %d backup overlaps with primary",
                     cfg->block_id);
            return FALSE;
        }

        return TRUE;
    }

    /* Check for Dataset block */
    if (cfg->block_type == NVM_BLOCK_DATASET) {
        if (cfg->dataset_count == 0 || cfg->dataset_count > 4) {
            LOG_ERROR("EEPROM: Dataset Block %d invalid count=%d",
                     cfg->block_id, cfg->dataset_count);
            return FALSE;
        }

        /* Calculate total space needed */
        uint32_t total_space = cfg->dataset_count * EEPROM_BLOCK_SLOT_SIZE;
        if (total_space > 4096) {  /* Max 4 slots for now */
            LOG_ERROR("EEPROM: Dataset Block %d needs %d bytes, exceeds limit",
                     cfg->block_id, total_space);
            return FALSE;
        }

        return TRUE;
    }

    return TRUE;
}

/**
 * @brief Calculate Dataset block version offset
 */
uint32_t EEPROM_DatasetVersionOffset(uint32_t base_offset, uint8_t dataset_index)
{
    return base_offset + (dataset_index * EEPROM_BLOCK_SLOT_SIZE);
}
