#ifndef TEST_ESP_EVENT_H
#define TEST_ESP_EVENT_H

#include <esp_err.h>

#include <stdint.h>

typedef const char* esp_event_base_t;
typedef void (*esp_event_handler_t)(void*, esp_event_base_t, int32_t, void*);
typedef void* esp_event_handler_instance_t;

#define WIFI_EVENT "wifi"
#define IP_EVENT "ip"
#define ESP_EVENT_ANY_ID (-1)
#define IP_EVENT_STA_GOT_IP 7

esp_err_t esp_event_loop_create_default(void);
esp_err_t esp_event_loop_delete_default(void);
esp_err_t esp_event_handler_instance_register(esp_event_base_t base, int32_t id, esp_event_handler_t handler,
                                              void* argument, esp_event_handler_instance_t* instance);
esp_err_t esp_event_handler_instance_unregister(esp_event_base_t base, int32_t id,
                                                esp_event_handler_instance_t instance);

#endif
