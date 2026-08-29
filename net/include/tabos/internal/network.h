#ifndef TABOS_INTERNAL_NETWORK_H
#define TABOS_INTERNAL_NETWORK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

typedef enum {
    NETWORK_OPERATION_OK = 0,
    NETWORK_OPERATION_INVALID,
    NETWORK_OPERATION_OFFLINE,
    NETWORK_OPERATION_NOT_FOUND,
    NETWORK_OPERATION_TIMEOUT,
    NETWORK_OPERATION_IO,
    NETWORK_OPERATION_UNSUPPORTED,
} network_operation_result_t;

typedef struct {
    uint32_t family;
    char text[46];
} network_address_t;

typedef struct {
    uint32_t sequence;
    uint32_t bytes;
    uint32_t round_trip_ms;
} network_echo_result_t;

bool network_service_init(void);
void network_service_update(void);
void network_service_shutdown(void);
bool network_service_connect(const char* ssid, const char* password, bool automatic);
bool network_service_disconnect(void);
bool network_service_status(network_status_t* status);
network_operation_result_t network_service_resolve(const char* hostname, uint32_t family, network_address_t* address);
network_operation_result_t network_service_echo(const network_address_t* address, uint16_t sequence,
                                                uint16_t payload_bytes, uint32_t timeout_ms,
                                                network_echo_result_t* result);
const char* network_state_name(network_state_t state);

#endif
