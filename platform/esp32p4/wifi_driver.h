#ifndef TABOS_ESP32P4_WIFI_DRIVER_H
#define TABOS_ESP32P4_WIFI_DRIVER_H

#include <esp_event.h>
#include <esp_netif.h>

#include <stdbool.h>

typedef struct {
        esp_netif_t* netif;
        esp_event_handler_instance_t wifi_handler;
        esp_event_handler_instance_t ip_handler;
        bool event_loop_owned;
        bool wifi_initialized;
        bool wifi_started;
} tab5_wifi_driver_t;

bool tab5_wifi_driver_init(tab5_wifi_driver_t* driver, const char* hostname, esp_event_handler_t event_handler);
void tab5_wifi_driver_shutdown(tab5_wifi_driver_t* driver);

#endif
