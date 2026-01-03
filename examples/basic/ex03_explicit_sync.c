/**
 * @file ex03_explicit_sync.c
 * @brief Example 03: Explicit Synchronization
 *
 * Learning Objectives:
 * - Understand explicit synchronization (ReadBlock/WriteBlock)
 * - Understand application-driven synchronization timing
 * - Understand job completion polling
 *
 * Design Reference: 05-使用示例与最佳实践.md §1.1
 *
 * Use Case: Application explicitly controls when to read/write
 * - Read specific configuration on demand
 * - Save settings when user changes them
 */

#include "nvm.h"
#include "os_scheduler.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>

#define BLOCK_USER_PREF_ID   2
#define BLOCK_USER_PREF_SIZE 256

/* RAM mirror for user preferences */
static uint8_t user_preferences[BLOCK_USER_PREF_SIZE];

/* ROM default */
static const uint8_t rom_default_prefs[BLOCK_USER_PREF_SIZE] = {
    ['v'] = 50,  /* Volume */
    ['b'] = 50,  /* Bass */
    ['t'] = 50,  /* Treble */
    [3 ... 255] = 0xFF
};

/**
 * @brief Application callback when settings are changed
 */
static void on_user_settings_changed(uint8_t new_volume, uint8_t new_bass)
{
    LOG_INFO("");
    LOG_INFO("=== User Settings Changed ===");
    LOG_INFO("  New volume: %u", new_volume);
    LOG_INFO("  New bass:   %u", new_bass);

    /* Update RAM mirror */
    user_preferences['v'] = new_volume;
    user_preferences['b'] = new_bass;

    /* Explicitly save to EEPROM */
    LOG_INFO("  Saving to EEPROM...");
    NvM_WriteBlock(BLOCK_USER_PREF_ID, user_preferences);

    /* Wait for completion */
    uint8_t job_result;
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(BLOCK_USER_PREF_ID, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    if (job_result == NVM_REQ_OK) {
        LOG_INFO("  ✓ Settings saved (%u iterations)", iterations);
    } else {
        LOG_ERROR("  ✗ Save failed (result=%u)", job_result);
    }
}

/**
 * @brief Demonstrate explicit synchronization
 */
static void demo_explicit_sync(void)
{
    LOG_INFO("========================================");
    LOG_INFO("  Example 03: Explicit Synchronization");
    LOG_INFO("========================================");
    LOG_INFO("");
    LOG_INFO("Use Case: Application controls sync timing");
    LOG_INFO("");

    /* Initialize */
    NvM_Init();
    OsScheduler_Init(16);

    /* Register Block with ROM fallback */
    NvM_BlockConfig_t pref_block = {
        .block_id = BLOCK_USER_PREF_ID,
        .block_size = BLOCK_USER_PREF_SIZE,
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
        .priority = 10,
        .is_immediate = FALSE,
        .is_write_protected = FALSE,
        .ram_mirror_ptr = user_preferences,
        .rom_block_ptr = rom_default_prefs,
        .rom_block_size = sizeof(rom_default_prefs),
        .eeprom_offset = 0x0800
    };

    if (NvM_RegisterBlock(&pref_block) != E_OK) {
        LOG_ERROR("Block registration failed");
        return;
    }
    LOG_INFO("✓ Block registered with ROM fallback");
    LOG_INFO("");

    /* Scenario 1: Read settings on startup */
    LOG_INFO("=== Scenario 1: Read Settings on Startup ===");
    LOG_INFO("Application: Loading user preferences...");

    memset(user_preferences, 0x00, BLOCK_USER_PREF_SIZE);
    NvM_ReadBlock(BLOCK_USER_PREF_ID, user_preferences);

    uint8_t job_result;
    uint32_t iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(BLOCK_USER_PREF_ID, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    LOG_INFO("✓ Loaded (iterations=%u)", iterations);
    LOG_INFO("  Volume: %u", user_preferences['v']);
    LOG_INFO("  Bass:   %u", user_preferences['b']);
    LOG_INFO("  Treble: %u", user_preferences['t']);
    LOG_INFO("");

    /* Scenario 2: User adjusts settings (explicit write) */
    LOG_INFO("=== Scenario 2: User Adjusts Settings ===");
    on_user_settings_changed(75, 60);  /* Change volume to 75, bass to 60 */
    LOG_INFO("");

    /* Scenario 3: Verify persistence (explicit read) */
    LOG_INFO("=== Scenario 3: Verify Persistence ===");
    LOG_INFO("Application: Re-loading settings to verify...");

    memset(user_preferences, 0x00, BLOCK_USER_PREF_SIZE);
    NvM_ReadBlock(BLOCK_USER_PREF_ID, user_preferences);

    iterations = 0;
    do {
        NvM_MainFunction();
        NvM_GetJobResult(BLOCK_USER_PREF_ID, &job_result);
        iterations++;
    } while (job_result == NVM_REQ_PENDING && iterations < 100);

    LOG_INFO("✓ Re-loaded (iterations=%u)", iterations);
    LOG_INFO("  Volume: %u (expected 75)", user_preferences['v']);
    LOG_INFO("  Bass:   %u (expected 60)", user_preferences['b']);

    if (user_preferences['v'] == 75 && user_preferences['b'] == 60) {
        LOG_INFO("✓ Persistence verified");
    } else {
        LOG_ERROR("✗ Persistence FAILED");
    }
    LOG_INFO("");

    /* Scenario 4: Another setting change */
    LOG_INFO("=== Scenario 4: Another Setting Change ===");
    on_user_settings_changed(80, 70);
    LOG_INFO("");

    LOG_INFO("========================================");
    LOG_INFO("  Key Takeaways");
    LOG_INFO("========================================");
    LOG_INFO("✓ Explicit sync: Application controls timing");
    LOG_INFO("✓ ReadBlock/WriteBlock: Asynchronous API");
    LOG_INFO("✓ Polling loop: Wait for NVM_REQ_OK");
    LOG_INFO("✓ Use case: Save only when data changes");
    LOG_INFO("========================================");
}

/**
 * @brief Main function
 */
int main(void)
{
    demo_explicit_sync();
    return 0;
}
