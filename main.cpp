#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "FreeRTOS.h"
#include "task.h"

#include "wifi.h"
#include "settings.h"
#include "control.h"

#define WATCHDOG_TIMEOUT_MS 5000 // Watchdog timeout in milliseconds

extern "C"
{
    void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
    {
        printf("Stack overflow in task: %s\n", pcTaskName);
        while (1)
            ; // Hang here for debugging.
    }

    void vApplicationMallocFailedHook(void)
    {
        printf("Malloc failed!\n");
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    void Default_Handler(void)
    {
        uint32_t irq_num;
        asm volatile("mrs %0, ipsr" : "=r"(irq_num));
        printf("Unhandled IRQ: %ld\n", irq_num & 0x1FF); // Extract IRQ number
        while (1)
            ;
    }
}

int main()
{
    stdio_init_all();
    printf("Starting FreeRTOS with Watchdog...\n");

    // Initialize the watchdog with a timeout, this will reset the system if not regularly kicked
    watchdog_enable(WATCHDOG_TIMEOUT_MS, 1);

    initSettings();
    initControl();
    initWifi();

    xTaskCreate(wifiTask, "WiFiTask", 4096, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(settingsTask, "SettingsTask", 256, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(ledTask, "LedTask", 256, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(controlTask, "ControlTask", 256, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(interactionTask, "InteractionTask", 256, NULL, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();

    // If the scheduler returns, this should never happen.
    printf("Ending main, which should not happen.\n");
    return 0;
}

void watchdog_kick_task(void *params)
{
    while (1)
    {
        // Feed the watchdog to reset its timer
        watchdog_update();

        // Delay for less than the watchdog timeout to ensure it's regularly reset
        vTaskDelay(pdMS_TO_TICKS(WATCHDOG_TIMEOUT_MS / 2));
    }
}
