#include <tabos/internal/runtime.h>
#include <tabos/internal/application.h>
#include "msc_control.h"
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
#include <stdio.h>
#include <string.h>

static const char* const TAG = TABOS_SYSTEM_LOG_TAG;
static bool msc_restart_requested;

static void update_runtime(platform_runtime_events_t events)
{
    kernel_runtime_update(events);
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
    tab5_test_control_update();
#endif
    if (!tab5_msc_control_take_request()) {
        return;
    }
    const tabos_app_descriptor_t* active = tabos_app_active();
    if (tabos_process_count() != 1U || active == NULL || strcmp(active->name, "shell") != 0) {
        puts("TABOS MSC BUSY: exit the foreground application first");
    } else if (kernel_runtime_request_system_action(PLATFORM_SYSTEM_ACTION_REBOOT)) {
        msc_restart_requested = true;
        puts("TABOS MSC OK");
    } else {
        puts("TABOS MSC BUSY: shutdown already requested");
    }
    (void) fflush(stdout);
}

static void run_usb_storage_mode(void)
{
    terminal_t terminal;
    if (!display_init() || !terminal_init(&terminal, display_framebuffer(), TABOS_TERMINAL_SCALE)) {
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
    (void) display_present();

    if (!tab5_usb_storage_start()) {
        terminal_write_line(&terminal, "");
        terminal_write_line(&terminal, "Could not start USB storage mode.");
        terminal_write_line(&terminal, "Restarting...");
        (void) display_present();
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    }
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
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
        ESP_LOGI(TAG, "USB storage boot requested");
        run_usb_storage_mode();
    }

    if (!kernel_runtime_start(true)) {
        ESP_LOGE(TAG, "%s runtime startup failed", TABOS_SYSTEM_NAME);
        kernel_runtime_shutdown();
        platform_shutdown();
        return;
    }

    ESP_LOGI(TAG, "%s %s bootstrapped on %s", TABOS_SYSTEM_NAME, kernel_runtime_version(), platform_name());

    if (!tab5_msc_control_start()) {
        ESP_LOGW(TAG, "Serial MSC control unavailable");
    }
    (void) platform_run(update_runtime, kernel_runtime_next_deadline);
    tab5_msc_control_stop();
    const platform_system_action_t action = kernel_runtime_take_system_action();
    kernel_runtime_shutdown();
    platform_shutdown();
    if (msc_restart_requested && action == PLATFORM_SYSTEM_ACTION_REBOOT) {
        tab5_usb_storage_request_next_boot();
    }
    platform_perform_system_action(action);
}
