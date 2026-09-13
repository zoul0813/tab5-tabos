#ifndef TEST_ESP_WIFI_H
#define TEST_ESP_WIFI_H

#include <esp_err.h>

typedef struct {
    int unused;
} wifi_init_config_t;

#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t) {0})
#define WIFI_MODE_STA 1

esp_err_t esp_wifi_init(const wifi_init_config_t* config);
esp_err_t esp_wifi_deinit(void);
esp_err_t esp_wifi_set_mode(int mode);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_stop(void);

#endif
