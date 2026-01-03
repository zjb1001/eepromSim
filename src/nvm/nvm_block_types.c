/**
 * @file nvm_block_types.c
 * @brief NvM Block Type Implementations (Native, Redundant, Dataset)
 *
 * REQ-Block管理: design/03-Block管理机制.md §2
 * - Native Block: 单副本
 * - Redundant Block: 双副本机制
 * - Dataset Block: 多版本管理
 */

#include "nvm.h"
#include "nvm_block_types.h"
#include "eeprom_layout.h"
#include "memif.h"
#include "crc16.h"
#include "logging.h"
#include <string.h>

/**
 * @brief Try to read block with CRC verification
 *
 * @param offset EEPROM offset
 * @param data Data buffer
 * @param size Block size
 * @param crc_type CRC type
 * @return TRUE if successful
 */
boolean NvM_TryReadBlock(uint32_t offset, uint8_t *data, uint16_t size, NvM_CrcType_t crc_type)
{
    /* Read data */
    if (MemIf_Read(offset, data, size) != E_OK) {
        return FALSE;
    }

    /* Verify CRC if needed */
    if (crc_type != NVM_CRC_NONE) {
        uint32_t crc_offset = offset + size;
        uint16_t stored_crc = 0;

        /* Read CRC from page-aligned location */
        if (MemIf_Read(crc_offset, (uint8_t*)&stored_crc, sizeof(stored_crc)) != E_OK) {
            LOG_DEBUG("NvM: CRC read failed at offset 0x%X", crc_offset);
            return FALSE;
        }

        uint16_t calculated_crc = Crc16_Calculate(data, size);

        if (stored_crc != calculated_crc) {
            LOG_DEBUG("NvM: CRC failed at offset 0x%X (stored=0x%04X, calc=0x%04X)",
                     offset, stored_crc, calculated_crc);
            return FALSE;
        }

        LOG_DEBUG("NvM: CRC OK at offset 0x%X (0x%04X)", offset, stored_crc);
    }

    return TRUE;
}

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
                                     uint16_t size, NvM_CrcType_t crc_type)
{
    uint16_t crc = 0;
    uint16_t crc_size = 0;

    /* Calculate CRC if needed */
    if (crc_type != NVM_CRC_NONE) {
        crc = Crc16_Calculate(data, size);
        crc_size = sizeof(crc);
        LOG_DEBUG("NvM: CRC = 0x%04X for offset 0x%X", crc, offset);
    }

    /* Erase block first */
    if (MemIf_Erase(offset, size) != E_OK) {
        LOG_ERROR("NvM: Erase failed at offset 0x%X", offset);
        return E_NOT_OK;
    }

    /* Write data */
    if (MemIf_Write(offset, data, size) != E_OK) {
        LOG_ERROR("NvM: Write failed at offset 0x%X", offset);
        return E_NOT_OK;
    }

    /* Write CRC - pad to page boundary for EEPROM write constraints */
    if (crc_type != NVM_CRC_NONE) {
        uint32_t crc_offset = offset + size;

        /* Check if CRC offset is page-aligned */
        if ((crc_offset % 256) == 0) {
            /* CRC is at page boundary - can write directly with padding */
            uint8_t page_buffer[256];
            memset(page_buffer, 0xFF, 256);  /* Fill with erased state */

            /* Copy CRC to start of page */
            memcpy(page_buffer, &crc, crc_size);

            /* Write entire page */
            if (MemIf_Write(crc_offset, page_buffer, 256) != E_OK) {
                LOG_ERROR("NvM: CRC page write failed at offset 0x%X", offset);
                return E_NOT_OK;
            }
        } else {
            /* CRC is within data page - this shouldn't happen with slot layout */
            LOG_ERROR("NvM: CRC at offset 0x%X is not page-aligned", crc_offset);
            return E_NOT_OK;
        }
    }

    return E_OK;
}

/**
 * @brief Read Native Block
 */
Std_ReturnType NvM_ReadNativeBlock(NvM_BlockConfig_t *block, void *data)
{
    if (NvM_TryReadBlock(block->eeprom_offset, (uint8_t*)data,
                        block->block_size, block->crc_type)) {
        block->state = NVM_BLOCKSTATE_VALID;
        return E_OK;
    }

    /* CRC failed, try ROM fallback */
    if (block->rom_block_ptr != NULL && block->rom_block_size > 0) {
        LOG_WARN("NvM: NATIVE block %d CRC failed, loading ROM default", block->block_id);
        memcpy(data, block->rom_block_ptr,
               (block->rom_block_size < block->block_size) ? block->rom_block_size : block->block_size);
        block->state = NVM_BLOCKSTATE_INVALID;
        return E_NOT_OK;
    }

    block->state = NVM_BLOCKSTATE_INVALID;
    return E_NOT_OK;
}

/**
 * @brief Write Native Block
 */
Std_ReturnType NvM_WriteNativeBlock(NvM_BlockConfig_t *block, const void *data)
{
    Std_ReturnType ret = NvM_WriteBlockWithCrc(block->eeprom_offset, (uint8_t*)data,
                                               block->block_size, block->crc_type);
    if (ret == E_OK) {
        block->erase_count++;
        block->state = NVM_BLOCKSTATE_VALID;
        LOG_INFO("NvM: NATIVE block %d written successfully", block->block_id);
    }

    return ret;
}

/**
 * @brief Read Redundant Block
 */
Std_ReturnType NvM_ReadRedundantBlock(NvM_BlockConfig_t *block, void *data)
{
    LOG_DEBUG("NvM: Reading REDUNDANT block %d", block->block_id);

    /* Try primary copy */
    if (NvM_TryReadBlock(block->eeprom_offset, (uint8_t*)data,
                        block->block_size, block->crc_type)) {
        LOG_INFO("NvM: REDUNDANT block %d primary copy OK", block->block_id);
        block->state = NVM_BLOCKSTATE_VALID;
        return E_OK;
    }

    /* Primary failed, try backup copy */
    LOG_WARN("NvM: REDUNDANT block %d primary failed, trying backup", block->block_id);
    if (NvM_TryReadBlock(block->redundant_eeprom_offset, (uint8_t*)data,
                        block->block_size, block->crc_type)) {
        LOG_INFO("NvM: REDUNDANT block %d backup copy OK (recovered)", block->block_id);
        block->state = NVM_BLOCKSTATE_RECOVERED;
        return E_OK;
    }

    /* Both failed, try ROM fallback */
    if (block->rom_block_ptr != NULL && block->rom_block_size > 0) {
        LOG_ERROR("NvM: REDUNDANT block %d both copies failed, loading ROM default", block->block_id);
        memcpy(data, block->rom_block_ptr,
               (block->rom_block_size < block->block_size) ? block->rom_block_size : block->block_size);
        block->state = NVM_BLOCKSTATE_INVALID;
        return E_NOT_OK;
    }

    LOG_ERROR("NvM: REDUNDANT block %d all copies failed", block->block_id);
    block->state = NVM_BLOCKSTATE_INVALID;
    return E_NOT_OK;
}

/**
 * @brief Write Redundant Block (write primary, then backup)
 */
Std_ReturnType NvM_WriteRedundantBlock(NvM_BlockConfig_t *block, const void *data)
{
    LOG_DEBUG("NvM: Writing REDUNDANT block %d", block->block_id);

    /* Write primary copy */
    Std_ReturnType ret = NvM_WriteBlockWithCrc(block->eeprom_offset, (uint8_t*)data,
                                               block->block_size, block->crc_type);
    if (ret != E_OK) {
        LOG_ERROR("NvM: REDUNDANT block %d primary write failed", block->block_id);
        return E_NOT_OK;
    }

    /* Verify primary copy */
    uint8_t verify_buffer[256];
    if (block->block_size <= 256) {
        if (!NvM_TryReadBlock(block->eeprom_offset, verify_buffer,
                             block->block_size, block->crc_type)) {
            LOG_ERROR("NvM: REDUNDANT block %d primary verification failed", block->block_id);
            return E_NOT_OK;
        }
    }

    /* Write backup copy */
    ret = NvM_WriteBlockWithCrc(block->redundant_eeprom_offset, (uint8_t*)data,
                               block->block_size, block->crc_type);
    if (ret != E_OK) {
        LOG_WARN("NvM: REDUNDANT block %d backup write failed (primary OK)", block->block_id);
        /* Continue anyway - primary is OK */
    }

    /* Update version number */
    block->active_version++;
    if (block->version_control_offset > 0) {
        MemIf_Write(block->version_control_offset, &block->active_version, 1);
    }

    block->erase_count++;
    block->state = NVM_BLOCKSTATE_VALID;
    LOG_INFO("NvM: REDUNDANT block %d written successfully (version=%u)",
             block->block_id, block->active_version);

    return E_OK;
}

/**
 * @brief Read Dataset Block
 */
Std_ReturnType NvM_ReadDatasetBlock(NvM_BlockConfig_t *block, void *data)
{
    LOG_DEBUG("NvM: Reading DATASET block %d (active=%u/%u)",
              block->block_id, block->active_dataset_index, block->dataset_count);

    /* Try active dataset first, then other versions */
    for (uint8_t i = 0; i < block->dataset_count; i++) {
        uint8_t dataset_index = (block->active_dataset_index + i) % block->dataset_count;
        /* Use fixed slot size for each dataset version */
        uint32_t offset = block->eeprom_offset + (dataset_index * EEPROM_BLOCK_SLOT_SIZE);

        if (NvM_TryReadBlock(offset, (uint8_t*)data, block->block_size, block->crc_type)) {
            if (i == 0) {
                LOG_INFO("NvM: DATASET block %d version %u OK", block->block_id, dataset_index);
                block->state = NVM_BLOCKSTATE_VALID;
            } else {
                LOG_WARN("NvM: DATASET block %d fell back to version %u", block->block_id, dataset_index);
                block->state = NVM_BLOCKSTATE_RECOVERED;
                block->active_dataset_index = dataset_index;
            }
            return E_OK;
        }
    }

    /* All versions failed, try ROM fallback */
    if (block->rom_block_ptr != NULL && block->rom_block_size > 0) {
        LOG_ERROR("NvM: DATASET block %d all versions failed, loading ROM default", block->block_id);
        memcpy(data, block->rom_block_ptr,
               (block->rom_block_size < block->block_size) ? block->rom_block_size : block->block_size);
        block->state = NVM_BLOCKSTATE_INVALID;
        return E_NOT_OK;
    }

    LOG_ERROR("NvM: DATASET block %d all versions failed", block->block_id);
    block->state = NVM_BLOCKSTATE_INVALID;
    return E_NOT_OK;
}

/**
 * @brief Write Dataset Block (round-robin)
 */
Std_ReturnType NvM_WriteDatasetBlock(NvM_BlockConfig_t *block, const void *data)
{
    LOG_DEBUG("NvM: Writing DATASET block %d", block->block_id);

    /* Move to next dataset slot */
    uint8_t next_index = (block->active_dataset_index + 1) % block->dataset_count;
    /* Use fixed slot size for each dataset version */
    uint32_t offset = block->eeprom_offset + (next_index * EEPROM_BLOCK_SLOT_SIZE);

    /* Write to new slot */
    Std_ReturnType ret = NvM_WriteBlockWithCrc(offset, (uint8_t*)data,
                                               block->block_size, block->crc_type);
    if (ret != E_OK) {
        LOG_ERROR("NvM: DATASET block %d write failed at slot %u", block->block_id, next_index);
        return E_NOT_OK;
    }

    /* Update active index */
    block->active_dataset_index = next_index;

    block->erase_count++;
    block->state = NVM_BLOCKSTATE_VALID;
    LOG_INFO("NvM: DATASET block %d written successfully (slot=%u/%u)",
             block->block_id, next_index, block->dataset_count);

    return E_OK;
}
