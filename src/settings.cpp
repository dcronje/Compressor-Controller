#include "settings.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/structs/xip_ctrl.h"

// Global settings variable
volatile Settings currentSettings = {
    .ssid = "",
    .password = "",
    .authMode = 0,
    .pressureTimeout = 60,
    .supplyTimeout = 5,
    .motorTimeout = 2,
    .magic = SETTINGS_MAGIC,
};

// Queue handle
QueueHandle_t settingsQueue = NULL;

// Utility: Invalidate XIP Cache
static void invalidateXipCache()
{
  xip_ctrl_hw->flush = 1;
  while (!(xip_ctrl_hw->stat & XIP_STAT_FLUSH_READY_BITS))
  {
    tight_loop_contents();
  }
  printf("XIP cache invalidated.\n");
}

// Flash erase helper
static void callFlashRangeErase(void *param)
{
  uint32_t offset = (uint32_t)param;
  flash_range_erase(offset, FLASH_SECTOR_SIZE);
}

// Flash program helper
static void callFlashRangeProgram(void *param)
{
  uint32_t offset = ((uintptr_t *)param)[0];
  const uint8_t *data = (const uint8_t *)((uintptr_t *)param)[1];
  flash_range_program(offset, data, FLASH_SECTOR_SIZE);
}

// Load settings from flash
bool loadSettingsFromFlash(Settings *settings)
{
  const uint8_t *flashMemory = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
  const Settings *flashSettings = (const Settings *)flashMemory;

  printf("Reading settings from flash...\n");

  if (flashSettings->magic == SETTINGS_MAGIC)
  {
    memcpy(settings, flashSettings, sizeof(Settings));
    printf("Loaded settings: SSID='%s', Auth Mode=%d\n", settings->ssid, settings->authMode);
    return true;
  }

  printf("No valid settings found in flash.\n");
  return false;
}

// Save settings to flash
void saveSettingsToFlash(const Settings *settings)
{
  printf("Saving settings to flash: SSID='%s', Auth Mode=%d\n", settings->ssid, settings->authMode);

  // Prepare buffer for writing
  uint8_t buffer[sizeof(Settings)];
  memcpy(buffer, settings, sizeof(Settings));

  // Safely erase flash
  int rc = flash_safe_execute(callFlashRangeErase, (void *)FLASH_TARGET_OFFSET, UINT32_MAX);
  if (rc != PICO_OK)
  {
    printf("Error erasing flash sector: %d\n", rc);
    return;
  }

  printf("Flash sector erased successfully.\n");

  // Safely write to flash
  uintptr_t params[] = {FLASH_TARGET_OFFSET, (uintptr_t)buffer};
  rc = flash_safe_execute(callFlashRangeProgram, params, UINT32_MAX);
  if (rc != PICO_OK)
  {
    printf("Error programming flash: %d\n", rc);
    return;
  }

  printf("Settings saved successfully. Verifying...\n");

  // Verify the written data
  const uint8_t *flashMemory = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
  if (memcmp(buffer, flashMemory, sizeof(Settings)) == 0)
  {
    printf("Settings verification successful.\n");
  }
  else
  {
    printf("Settings verification failed.\n");
  }
}

// Reset settings in flash
void resetSettings()
{
  printf("Resetting settings in flash...\n");

  // Default settings
  Settings defaultSettings = {
      .ssid = "",
      .password = "",
      .authMode = 0,
      .pressureTimeout = currentSettings.pressureTimeout, // DO NOT RESET
      .supplyTimeout = currentSettings.supplyTimeout,     // DO NOT RESET
      .motorTimeout = currentSettings.motorTimeout,       // DO NOT RESET
      .magic = SETTINGS_MAGIC,
  };

  saveSettingsToFlash(&defaultSettings);
}

// Initialize settings
void initSettings()
{
  Settings loadedSettings;

  if (loadSettingsFromFlash(&loadedSettings))
  {
    memcpy((Settings *)&currentSettings, &loadedSettings, sizeof(Settings));
  }
  else
  {
    printf("No valid settings found. Using defaults.\n");
    memset((Settings *)&currentSettings, 0, sizeof(Settings));
    currentSettings.magic = SETTINGS_MAGIC;
  }

  printf("Current settings: SSID='%s', Auth Mode=%d\n", currentSettings.ssid, currentSettings.authMode);

  // Create the settings queue
  settingsQueue = xQueueCreate(5, sizeof(SettingsCommand));
  if (settingsQueue == NULL)
  {
    printf("Failed to create settings queue.\n");
  }
}

// Request settings validation
void requestSettingsValidation()
{
  SettingsCommand command = {
      .type = SETTINGS_UPDATE,
  };

  memcpy((Settings *)&command.data, (Settings *)&currentSettings, sizeof(Settings));

  if (xQueueSend(settingsQueue, &command, portMAX_DELAY) != pdPASS)
  {
    printf("Settings validation request failed (queue full).\n");
  }
}

// Request settings reset
void requestSettingsReset()
{
  SettingsCommand command = {
      .type = SETTINGS_RESET,
  };
  if (xQueueSend(settingsQueue, &command, portMAX_DELAY) != pdPASS)
  {
    printf("Settings reset request failed (queue full).\n");
  }
}

// Settings task
void settingsTask(void *params)
{
  SettingsCommand command;

  while (1)
  {
    if (xQueueReceive(settingsQueue, &command, portMAX_DELAY))
    {
      if (command.type == SETTINGS_UPDATE)
      {
        printf("Processing settings update.\n");
        saveSettingsToFlash(&command.data);
        memcpy((Settings *)&currentSettings, &command.data, sizeof(Settings));
      }
      else if (command.type == SETTINGS_RESET)
      {
        printf("Processing settings reset.\n");
        resetSettings();
        memset((Settings *)&currentSettings, 0, sizeof(Settings));
        currentSettings.magic = SETTINGS_MAGIC;
      }
    }
  }
}
