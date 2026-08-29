#ifndef TABOS_INTERNAL_NETWORK_H
#define TABOS_INTERNAL_NETWORK_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    NETWORK_STATE_OFFLINE = 0,
    NETWORK_STATE_STARTING,
    NETWORK_STATE_SCANNING,
    NETWORK_STATE_CONNECTING,
    NETWORK_STATE_ONLINE,
    NETWORK_STATE_DISCONNECTING,
    NETWORK_STATE_FAILED,
} network_state_t;

typedef struct {
        network_state_t state;
        char hostname[33];
        char ssid[33];
        char ipv4[16];
        int signal_dbm;
        unsigned int attempts;
        bool auto_connect;
        bool config_available;
        char last_failure[64];
} network_status_t;

bool network_service_init(void);
void network_service_update(void);
void network_service_shutdown(void);
bool network_service_connect(const char* ssid, const char* password, bool automatic);
bool network_service_disconnect(void);
bool network_service_status(network_status_t* status);
const char* network_state_name(network_state_t state);

#endif
