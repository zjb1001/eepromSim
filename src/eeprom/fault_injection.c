/**
 * @file fault_injection.c
 * @brief Fault Injection Framework Implementation
 *
 * REQ-故障注入: design/04-数据完整性方案.md §5
 * - Fault enable/disable management
 * - Probability-based triggering
 * - Block-specific targeting
 * - Statistics tracking
 */

#include "fault_injection.h"
#include "logging.h"
#include <string.h>

/**
 * @brief Maximum number of fault configurations
 */
#define FAULT_MAX_CONFIGS 16

/**
 * @brief Fault configuration table
 */
static FaultConfig_t g_fault_configs[FAULT_MAX_CONFIGS];

/**
 * @brief Fault statistics
 */
static FaultStats_t g_stats = {0};

/**
 * @brief Random number generator state (simple LCG)
 */
static uint32_t g_rand_state = 12345;

/**
 * @brief Simple pseudo-random number generator
 *
 * @return Random number 0-999
 */
static uint16_t random_percent(void)
{
    g_rand_state = g_rand_state * 1103515245 + 12345;
    return (uint16_t)((g_rand_state / 65536) % 1000);
}

/**
 * @brief Find fault configuration by ID
 */
static FaultConfig_t* find_config(FaultId_t fault_id)
{
    for (int i = 0; i < FAULT_MAX_CONFIGS; i++) {
        if (g_fault_configs[i].fault_id == fault_id && g_fault_configs[i].fault_id != FAULT_NONE) {
            return &g_fault_configs[i];
        }
    }
    return NULL;
}

/**
 * @brief Find or create fault configuration
 */
static FaultConfig_t* find_or_create_config(FaultId_t fault_id)
{
    /* Try to find existing */
    FaultConfig_t *config = find_config(fault_id);
    if (config != NULL) {
        return config;
    }

    /* Create new config */
    for (int i = 0; i < FAULT_MAX_CONFIGS; i++) {
        if (g_fault_configs[i].fault_id == FAULT_NONE) {
            g_fault_configs[i].fault_id = fault_id;
            g_fault_configs[i].enabled = FALSE;
            g_fault_configs[i].target_block_id = 0xFF;
            g_fault_configs[i].trigger_count = 0;
            g_fault_configs[i].triggered_count = 0;
            g_fault_configs[i].probability_percent = 0;
            return &g_fault_configs[i];
        }
    }

    return NULL;
}

/**
 * @brief Check if fault should trigger based on probability and count
 */
static boolean should_trigger(FaultConfig_t *config)
{
    if (!config->enabled) {
        return FALSE;
    }

    /* Check trigger count limit */
    if (config->trigger_count > 0 && config->triggered_count >= config->trigger_count) {
        return FALSE;
    }

    /* Check probability (0 = always trigger, 100 = never trigger) */
    if (config->probability_percent == 0) {
        return TRUE;
    }

    uint16_t roll = random_percent();
    if (roll < config->probability_percent * 10) {
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief Initialize fault injection framework
 */
void FaultInj_Init(void)
{
    memset(g_fault_configs, 0, sizeof(g_fault_configs));
    memset(&g_stats, 0, sizeof(g_stats));

    LOG_INFO("FaultInj: Initialized (max_configs=%d)", FAULT_MAX_CONFIGS);
}

/**
 * @brief Enable a specific fault
 */
Std_ReturnType FaultInj_Enable(FaultId_t fault_id)
{
    if (fault_id == FAULT_NONE || fault_id >= FAULT_MAX_ID) {
        return E_NOT_OK;
    }

    FaultConfig_t *config = find_or_create_config(fault_id);
    if (config == NULL) {
        LOG_ERROR("FaultInj: Cannot enable fault %d (config table full)", fault_id);
        return E_NOT_OK;
    }

    config->enabled = TRUE;
    LOG_INFO("FaultInj: Enabled fault %d", fault_id);

    return E_OK;
}

/**
 * @brief Disable a specific fault
 */
Std_ReturnType FaultInj_Disable(FaultId_t fault_id)
{
    FaultConfig_t *config = find_config(fault_id);
    if (config == NULL) {
        return E_NOT_OK;
    }

    config->enabled = FALSE;
    LOG_INFO("FaultInj: Disabled fault %d", fault_id);

    return E_OK;
}

/**
 * @brief Check if a fault is enabled
 */
boolean FaultInj_IsEnabled(FaultId_t fault_id)
{
    FaultConfig_t *config = find_config(fault_id);
    if (config == NULL) {
        return FALSE;
    }

    return config->enabled;
}

/**
 * @brief Configure fault parameters
 */
Std_ReturnType FaultInj_Configure(const FaultConfig_t *config)
{
    if (config == NULL || config->fault_id == FAULT_NONE) {
        return E_NOT_OK;
    }

    FaultConfig_t *target = find_or_create_config(config->fault_id);
    if (target == NULL) {
        return E_NOT_OK;
    }

    *target = *config;
    target->triggered_count = 0;  /* Reset counter on reconfigure */

    LOG_INFO("FaultInj: Configured fault %d (block=%d, prob=%u%%, count=%u)",
             config->fault_id, config->target_block_id,
             config->probability_percent, config->trigger_count);

    return E_OK;
}

/**
 * @brief Get fault statistics
 */
Std_ReturnType FaultInj_GetStats(FaultStats_t *stats)
{
    if (stats == NULL) {
        return E_NOT_OK;
    }

    *stats = g_stats;
    return E_OK;
}

/**
 * @brief Reset fault statistics
 */
void FaultInj_ResetStats(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
    LOG_INFO("FaultInj: Statistics reset");
}

/**
 * @brief Reset all fault configurations
 */
void FaultInj_ResetAll(void)
{
    memset(g_fault_configs, 0, sizeof(g_fault_configs));
    LOG_INFO("FaultInj: All configurations reset");
}

/**
 * @brief Hook: Called before EEPROM read
 */
boolean FaultInj_HookBeforeRead(uint32_t address, uint32_t length)
{
    (void)address;
    (void)length;

    /* No faults to inject before read */
    return FALSE;
}

/**
 * @brief Hook: Called after EEPROM read (bit flip injection)
 */
boolean FaultInj_HookAfterRead(uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0) {
        return FALSE;
    }

    /* Check for P0-03: Single bit flip */
    FaultConfig_t *config = find_config(FAULT_P0_BITFLIP_SINGLE);
    if (config != NULL && should_trigger(config)) {
        /* Flip single bit in first byte */
        data[0] ^= 0x01;
        config->triggered_count++;
        g_stats.total_injected++;

        LOG_WARN("FaultInj: Injected single bit flip at offset 0 (0x%02X -> 0x%02X)",
                 data[0] ^ 0x01, data[0]);
        return TRUE;
    }

    /* Check for P0-04: Multiple bit flip */
    config = find_config(FAULT_P0_BITFLIP_MULTI);
    if (config != NULL && should_trigger(config)) {
        /* Flip multiple bits in first few bytes */
        uint32_t flip_count = (length > 4) ? 4 : length;
        for (uint32_t i = 0; i < flip_count; i++) {
            data[i] ^= 0xFF;
        }
        config->triggered_count++;
        g_stats.total_injected++;

        LOG_WARN("FaultInj: Injected multi-bit flip in first %u bytes", flip_count);
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief Hook: Called before EEPROM write
 */
boolean FaultInj_HookBeforeWrite(uint32_t address, uint32_t length)
{
    (void)address;
    (void)length;

    /* Check for P0-06: Erase timeout */
    FaultConfig_t *config = find_config(FAULT_P0_TIMEOUT_ERASE);
    if (config != NULL && should_trigger(config)) {
        config->triggered_count++;
        g_stats.total_injected++;

        LOG_WARN("FaultInj: Injected erase timeout at address 0x%X", address);
        return TRUE;  /* Block the write */
    }

    return FALSE;
}

/**
 * @brief Hook: Called after EEPROM write (power loss injection)
 */
boolean FaultInj_HookAfterWrite(uint32_t address)
{
    /* Check for P0-01: Power loss during page program */
    FaultConfig_t *config = find_config(FAULT_P0_POWERLOSS_PAGEPROGRAM);
    if (config != NULL && should_trigger(config)) {
        config->triggered_count++;
        g_stats.total_injected++;

        LOG_ERROR("FaultInj: Injected power loss after write at 0x%X", address);
        return TRUE;  /* Simulate power loss */
    }

    return FALSE;
}

/**
 * @brief Hook: Called for CRC calculation
 */
boolean FaultInj_HookCrc(const uint8_t *data, uint32_t length, uint16_t *crc)
{
    if (crc == NULL) {
        return FALSE;
    }

    (void)data;
    (void)length;

    /* Check for P0-07: CRC inversion */
    FaultConfig_t *config = find_config(FAULT_P0_CRC_INVERT);
    if (config != NULL && should_trigger(config)) {
        *crc = ~(*crc);
        config->triggered_count++;
        g_stats.total_injected++;

        LOG_WARN("FaultInj: Injected CRC inversion (0x%04X -> 0x%04X)",
                 ~(*crc), *crc);
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief Hook: Called for write verification
 */
boolean FaultInj_HookVerify(uint32_t address, const uint8_t *expected_data,
                            uint8_t *actual_data, uint32_t length)
{
    if (actual_data == NULL || expected_data == NULL) {
        return FALSE;
    }

    (void)address;

    /* Check for P0-08: Write verify always fail */
    FaultConfig_t *config = find_config(FAULT_P0_WRITE_VERIFY_FAIL);
    if (config != NULL && should_trigger(config)) {
        /* Corrupt actual data to cause verification failure */
        if (length > 0) {
            actual_data[0] = ~expected_data[0];
        }
        config->triggered_count++;
        g_stats.total_injected++;

        LOG_WARN("FaultInj: Injected verification failure at 0x%X", address);
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief Hook: Called before RAM mirror read
 */
boolean FaultInj_HookRamMirror(uint8_t block_id, uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0) {
        return FALSE;
    }

    /* Check for P0-09: RAM corruption */
    FaultConfig_t *config = find_config(FAULT_P0_RAM_CORRUPT);

    /* Check if this block is targeted */
    if (config != NULL && should_trigger(config)) {
        if (config->target_block_id == 0xFF || config->target_block_id == block_id) {
            /* Corrupt data */
            memset(data, 0xAA, length);
            config->triggered_count++;
            g_stats.total_injected++;

            LOG_WARN("FaultInj: Injected RAM corruption for block %d", block_id);
            return TRUE;
        }
    }

    return FALSE;
}
