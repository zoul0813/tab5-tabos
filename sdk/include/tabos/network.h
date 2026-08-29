#ifndef TABOS_NETWORK_H
#define TABOS_NETWORK_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TABOS_NETWORK_OFFLINE = 0,
    TABOS_NETWORK_STARTING,
    TABOS_NETWORK_SCANNING,
    TABOS_NETWORK_CONNECTING,
    TABOS_NETWORK_ONLINE,
    TABOS_NETWORK_DISCONNECTING,
    TABOS_NETWORK_FAILED,
} tabos_network_state_t;

typedef struct {
        tabos_network_state_t state;
        char hostname[33];
        char ssid[33];
        char ipv4[16];
        int32_t signal_dbm;
        uint32_t attempts;
        bool auto_connect;
        bool saved_config;
        char last_failure[96];
} tabos_network_status_t;

int tabos_network_get_status(tabos_network_status_t* status);
int tabos_network_connect_saved(void);
int tabos_network_disconnect(void);
const char* tabos_network_state_name(tabos_network_state_t state);

#endif
