#ifndef TABOS_ESP32P4_WIFI_STARTUP_H
#define TABOS_ESP32P4_WIFI_STARTUP_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <stdatomic.h>
#include <stdbool.h>

typedef struct {
        SemaphoreHandle_t done;
        atomic_bool running;
        atomic_bool cancel_requested;
} tab5_wifi_startup_t;

bool tab5_wifi_startup_prepare(tab5_wifi_startup_t* startup);
bool tab5_wifi_startup_running(const tab5_wifi_startup_t* startup);
bool tab5_wifi_startup_cancelled(const tab5_wifi_startup_t* startup);
void tab5_wifi_startup_finish(tab5_wifi_startup_t* startup);
void tab5_wifi_startup_stop_and_join(tab5_wifi_startup_t* startup);

#endif
