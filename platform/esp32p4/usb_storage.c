#include "internal.h"

#include <tabos/platform/esp32p4.h>

#include <bsp/m5stack_tab5.h>
#include <driver/sdmmc_host.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <sd_pwr_ctrl_by_on_chip_ldo.h>
#include <sdmmc_cmd.h>
#include <tinyusb.h>
#include <tinyusb_default_config.h>
#include <tinyusb_msc.h>

#include <stdlib.h>

static const char *const TAG = "tabos_usb_storage";
static sdmmc_card_t *storage_card;
static sd_pwr_ctrl_handle_t storage_power;
static tinyusb_msc_storage_handle_t storage_handle;
static bool usb_attached;
static bool restart_requested;

bool platform_usb_port_disable_host_power(void)
{
    const esp_err_t result = bsp_feature_enable(BSP_FEATURE_USB, false);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not disable USB-A host power: %s", esp_err_to_name(result));
        return false;
    }
    return true;
}

bool tab5_boot_usb_storage_requested(uint32_t window_ms)
{
    return tab5_keyboard_delete_held(window_ms);
}

static void request_restart(void)
{
    if (restart_requested) return;
    restart_requested = true;
    ESP_LOGI(TAG, "USB storage session ended; restarting Tab5");
    esp_restart();
}

static void storage_event(tinyusb_msc_storage_handle_t handle,
                          tinyusb_msc_event_t *event, void *arg)
{
    (void)handle;
    (void)arg;
    if (event == NULL || !usb_attached ||
        event->mount_point != TINYUSB_MSC_STORAGE_MOUNT_APP) return;
    if (event->id == TINYUSB_MSC_EVENT_MOUNT_COMPLETE ||
        event->id == TINYUSB_MSC_EVENT_MOUNT_FAILED ||
        event->id == TINYUSB_MSC_EVENT_FORMAT_REQUIRED ||
        event->id == TINYUSB_MSC_EVENT_FORMAT_FAILED) request_restart();
}

static void usb_event(tinyusb_event_t *event, void *arg)
{
    (void)arg;
    if (event == NULL) return;
    if (event->id == TINYUSB_EVENT_ATTACHED) {
        usb_attached = true;
    } else if (event->id == TINYUSB_EVENT_DETACHED && usb_attached) {
        request_restart();
    }
}

static esp_err_t initialize_card(void)
{
    sdmmc_host_t host;
    sdmmc_slot_config_t slot;
    bsp_sdcard_get_sdmmc_host(SDMMC_HOST_SLOT_0, &host);
    bsp_sdcard_sdmmc_get_slot(SDMMC_HOST_SLOT_0, &slot);

    const sd_pwr_ctrl_ldo_config_t power_config = {.ldo_chan_id = 4};
    esp_err_t result = sd_pwr_ctrl_new_on_chip_ldo(&power_config, &storage_power);
    if (result != ESP_OK) return result;
    host.pwr_ctrl_handle = storage_power;

    storage_card = calloc(1U, sizeof(*storage_card));
    if (storage_card == NULL) return ESP_ERR_NO_MEM;
    result = host.init();
    if (result != ESP_OK) return result;
    result = sdmmc_host_init_slot(host.slot, &slot);
    if (result != ESP_OK) return result;
    result = sdmmc_card_init(&host, storage_card);
    if (result == ESP_OK) sdmmc_card_print_info(stdout, storage_card);
    return result;
}

bool tab5_usb_storage_start(void)
{
    /*
     * The ESP32-P4 high-speed OTG controller is wired to the Tab5 USB-A
     * connector.  Remove the board's host-mode 5 V supply before turning that
     * connector into a device port; VBUS must come from the attached host.
     */
    if (!platform_usb_port_disable_host_power()) return false;

    esp_err_t result = initialize_card();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize microSD: %s", esp_err_to_name(result));
        return false;
    }

    const tinyusb_msc_driver_config_t msc_config = {
        .callback = storage_event,
    };
    result = tinyusb_msc_install_driver(&msc_config);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize USB MSC: %s", esp_err_to_name(result));
        return false;
    }

    const tinyusb_msc_storage_config_t storage_config = {
        .medium.card = storage_card,
        .fat_fs = {
            .base_path = BSP_SD_MOUNT_POINT,
            .config.max_files = 5,
            .do_not_format = true,
        },
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB,
    };
    result = tinyusb_msc_new_storage_sdmmc(&storage_config, &storage_handle);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not expose microSD through USB MSC: %s", esp_err_to_name(result));
        return false;
    }

    tinyusb_config_t usb_config = TINYUSB_DEFAULT_CONFIG(usb_event, NULL);
    result = tinyusb_driver_install(&usb_config);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not start USB device: %s", esp_err_to_name(result));
        return false;
    }
    ESP_LOGI(TAG, "T: exposed through USB-A as mass storage; waiting for safe eject");
    return true;
}
