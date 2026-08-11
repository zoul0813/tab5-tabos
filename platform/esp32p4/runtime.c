#include "internal.h"

#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>

#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>

#include <stdio.h>

static const char *const TAG = TABOS_PLATFORM_LOG_TAG;

bool tab_platform_init(bool headless)
{
    (void)headless;
    (void)tab_esp32p4_keyboard_init();
    return true;
}

int tab_platform_run(tab_platform_update_fn update)
{
    ESP_LOGI(TAG, "Tab5 platform run loop started");
    for (;;) {
        tab_esp32p4_keyboard_poll();
        if (update != NULL) update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void tab_platform_shutdown(void)
{
    tab_platform_display_shutdown();
    tab_esp32p4_keyboard_shutdown();
}

const char *tab_platform_name(void)
{
    return TABOS_TARGET_NAME_TAB5;
}

bool tab_platform_get_diagnostics(tab_platform_diagnostics_t *diagnostics)
{
    if (diagnostics == NULL) return false;
    uint32_t flash_capacity = 0U;
    (void)esp_flash_get_size(NULL, &flash_capacity);
    *diagnostics = (tab_platform_diagnostics_t){
        .device_name = "ESP32-P4",
        .cpu_cores = 2U,
        .cpu_frequency_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .memory_total_bytes = heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
        .memory_free_bytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        .memory_free_known = true,
        .external_memory_total_bytes = heap_caps_get_total_size(MALLOC_CAP_SPIRAM),
        .external_memory_free_bytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        .external_memory_present = true,
        .flash_capacity_bytes = flash_capacity,
        .keyboard_name = tab_esp32p4_keyboard_name(),
        .keyboard_present = tab_esp32p4_keyboard_present(),
    };
    return true;
}

void tab_platform_log(const char *message)
{
    if (message != NULL) ESP_LOGI(TABOS_SYSTEM_LOG_TAG, "%s", message);
}

uint64_t tab_platform_time_ms(void)
{
    return (uint64_t)esp_timer_get_time() / 1000U;
}

void tab_platform_input_wait(void)
{
    vTaskDelay(pdMS_TO_TICKS(1));
}
