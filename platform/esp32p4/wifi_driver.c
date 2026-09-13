#include "wifi_driver.h"

#include <esp_wifi.h>
#include <esp_wifi_default.h>

#include <string.h>

void tab5_wifi_driver_shutdown(tab5_wifi_driver_t* driver)
{
    if (driver == NULL) {
        return;
    }
    if (driver->wifi_started) {
        (void) esp_wifi_stop();
    }
    if (driver->ip_handler != NULL) {
        (void) esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, driver->ip_handler);
    }
    if (driver->wifi_handler != NULL) {
        (void) esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, driver->wifi_handler);
    }
    if (driver->wifi_initialized) {
        (void) esp_wifi_deinit();
    }
    if (driver->netif != NULL) {
        esp_netif_destroy_default_wifi(driver->netif);
    }
    if (driver->event_loop_owned) {
        (void) esp_event_loop_delete_default();
    }
    memset(driver, 0, sizeof(*driver));
}

bool tab5_wifi_driver_init(tab5_wifi_driver_t* driver, const char* hostname, esp_event_handler_t event_handler)
{
    if (driver == NULL || hostname == NULL || event_handler == NULL) {
        return false;
    }
    *driver = (tab5_wifi_driver_t) {0};

    if (esp_netif_init() != ESP_OK) {
        return false;
    }

    const esp_err_t loop_result = esp_event_loop_create_default();
    if (loop_result == ESP_OK) {
        driver->event_loop_owned = true;
    } else if (loop_result != ESP_ERR_INVALID_STATE) {
        tab5_wifi_driver_shutdown(driver);
        return false;
    }

    driver->netif = esp_netif_create_default_wifi_sta();
    if (driver->netif == NULL || esp_netif_set_hostname(driver->netif, hostname) != ESP_OK) {
        tab5_wifi_driver_shutdown(driver);
        return false;
    }

    const wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&init) != ESP_OK) {
        tab5_wifi_driver_shutdown(driver);
        return false;
    }
    driver->wifi_initialized = true;

    if (esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL, &driver->wifi_handler) !=
            ESP_OK ||
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL, &driver->ip_handler) !=
            ESP_OK ||
        esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
        tab5_wifi_driver_shutdown(driver);
        return false;
    }
    driver->wifi_started = true;
    if (esp_wifi_start() != ESP_OK) {
        tab5_wifi_driver_shutdown(driver);
        return false;
    }
    return true;
}
