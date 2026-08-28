#include <tabos/platform/platform.h>

#include <bsp/esp-bsp.h>
#include <esp_io_expander.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* const TAG = "tabos_power";

static void power_off(void)
{
    esp_io_expander_handle_t expander = bsp_io_expander1_init();
    if (expander != NULL) {
        const uint32_t signal = IO_EXPANDER_PIN_NUM_4;
        (void) esp_io_expander_set_dir(expander, signal, IO_EXPANDER_OUTPUT);
        (void) esp_io_expander_set_output_mode(expander, signal, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL);
        for (unsigned int attempt = 0U; attempt < 3U; ++attempt) {
            (void) esp_io_expander_set_level(expander, signal, 1U);
            vTaskDelay(pdMS_TO_TICKS(100U));
            (void) esp_io_expander_set_level(expander, signal, 0U);
            vTaskDelay(pdMS_TO_TICKS(100U));
        }
    } else {
        ESP_LOGE(TAG, "Could not initialize Tab5 power control");
    }
    ESP_LOGW(TAG, "Power remains present; Tab5 halted with services stopped");
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

void platform_perform_system_action(platform_system_action_t action)
{
    if (action == PLATFORM_SYSTEM_ACTION_REBOOT) {
        ESP_LOGI(TAG, "Restarting Tab5");
        esp_restart();
    }
    if (action == PLATFORM_SYSTEM_ACTION_POWER_OFF) {
        ESP_LOGI(TAG, "Powering off Tab5");
        power_off();
    }
}
