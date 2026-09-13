#ifndef TEST_ESP_NETIF_H
#define TEST_ESP_NETIF_H

#include <esp_err.h>

typedef struct esp_netif esp_netif_t;

esp_err_t esp_netif_init(void);
esp_netif_t* esp_netif_create_default_wifi_sta(void);
esp_err_t esp_netif_set_hostname(esp_netif_t* netif, const char* hostname);

#endif
