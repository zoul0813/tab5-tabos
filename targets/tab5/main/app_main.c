#include <tabos/internal/runtime.h>
#include <tabos/internal/display.h>
#include <tabos/internal/terminal.h>
#include <tabos/platform/platform.h>
#include <tabos/platform/esp32p4.h>

#include <tabos/config/identity.h>
#include <tabos/config/display.h>

#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *const TAG = TABOS_SYSTEM_LOG_TAG;

static void run_usb_storage_mode(void)
{
    terminal_t terminal;
    if (!display_init() ||
        !terminal_init(&terminal, display_framebuffer(), TABOS_TERMINAL_SCALE)) {
        ESP_LOGE(TAG, "Could not display USB storage status");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }
    terminal_clear(&terminal);
    terminal_set_cursor_visible(&terminal, false);
    terminal_write_line(&terminal, "TabOS USB Mass Storage Mode");
    terminal_write_line(&terminal, "");
    terminal_write_line(&terminal, "Connect Tab5 USB-A to a host USB-C port");
    terminal_write_line(&terminal, "with a USB-A-to-C data cable.");
    terminal_write_line(&terminal, "T: microSD will appear as a removable disk.");
    terminal_write_line(&terminal, "Eject the disk safely when finished.");
    terminal_write_line(&terminal, "Tab5 will restart automatically after eject.");
    (void)display_present();

    if (!tab5_usb_storage_start()) {
        terminal_write_line(&terminal, "");
        terminal_write_line(&terminal, "Could not start USB storage mode.");
        terminal_write_line(&terminal, "Restarting...");
        (void)display_present();
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    }
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

void app_main(void)
{
    if (!kernel_runtime_init()) {
        ESP_LOGE(TAG, "%s runtime initialization failed", TABOS_SYSTEM_NAME);
        return;
    }

    if (!platform_init(false)) {
        ESP_LOGE(TAG, "%s platform initialization failed", TABOS_SYSTEM_NAME);
        kernel_runtime_shutdown();
        return;
    }

    if (tab5_boot_usb_storage_requested(750U)) {
        ESP_LOGI(TAG, "Delete held during boot; entering USB storage mode");
        run_usb_storage_mode();
    }

    if (!kernel_runtime_start()) {
        ESP_LOGE(TAG, "%s display initialization failed", TABOS_SYSTEM_NAME);
        kernel_runtime_shutdown();
        platform_shutdown();
        return;
    }

    ESP_LOGI(
        TAG,
        "%s %s bootstrapped on %s",
        TABOS_SYSTEM_NAME,
        kernel_runtime_version(),
        platform_name()
    );

    (void)platform_run(kernel_runtime_update);
}
