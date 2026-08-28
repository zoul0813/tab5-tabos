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

static const char* const TAG = TABOS_PLATFORM_LOG_TAG;

bool platform_init(bool headless)
{
    (void) headless;
    if (!platform_usb_port_disable_host_power()) {
        ESP_LOGE(TAG, "Could not place USB-A port in safe unpowered state");
        return false;
    }
    (void) tab5_keyboard_init();
    (void) tab5_rtc_init();
    return true;
}

int platform_run(platform_update_fn update)
{
    ESP_LOGI(TAG, "Tab5 platform run loop started");
    for (;;) {
        tab5_keyboard_poll();
        if (update != NULL) {
            update();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void platform_shutdown(void)
{
    platform_display_shutdown();
    tab5_keyboard_shutdown();
    tab5_rtc_shutdown();
}

const char* platform_name(void)
{
    return TABOS_TARGET_NAME_TAB5;
}

bool platform_get_diagnostics(platform_diagnostics_t* diagnostics)
{
    if (diagnostics == NULL) {
        return false;
    }
    uint32_t flash_capacity = 0U;
    (void) esp_flash_get_size(NULL, &flash_capacity);
    *diagnostics = (platform_diagnostics_t) {
        .device_name                 = "ESP32-P4",
        .cpu_cores                   = 2U,
        .cpu_frequency_mhz           = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .memory_total_bytes          = heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
        .memory_free_bytes           = heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        .memory_free_known           = true,
        .external_memory_total_bytes = heap_caps_get_total_size(MALLOC_CAP_SPIRAM),
        .external_memory_free_bytes  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        .external_memory_present     = true,
        .flash_capacity_bytes        = flash_capacity,
        .keyboard_name               = tab5_keyboard_name(),
        .keyboard_present            = tab5_keyboard_present(),
        .rtc_name                    = "RX8130",
        .rtc_present                 = tab5_rtc_present(),
    };
    return true;
}

void platform_log(const char* message)
{
    if (message != NULL) {
        ESP_LOGI(TABOS_SYSTEM_LOG_TAG, "%s", message);
    }
}

uint64_t platform_time_ms(void)
{
    return (uint64_t) esp_timer_get_time() / 1000U;
}

void platform_input_wait(void)
{
    /* One millisecond rounds to zero at the configured 100 Hz tick rate. */
    vTaskDelay(1);
}
