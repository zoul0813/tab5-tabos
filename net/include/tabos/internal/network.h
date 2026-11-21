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
        /* SSID of the saved profile; empty when no network is saved. It differs
         * from ssid after a volatile connection to another network. */
        char saved_ssid[33];
} network_status_t;

typedef enum {
    NETWORK_OPERATION_OK = 0,
    NETWORK_OPERATION_INVALID,
    NETWORK_OPERATION_OFFLINE,
    NETWORK_OPERATION_NOT_FOUND,
    NETWORK_OPERATION_TIMEOUT,
    NETWORK_OPERATION_IO,
    NETWORK_OPERATION_UNSUPPORTED,
    NETWORK_OPERATION_BUSY,
} network_operation_result_t;

enum {
    NETWORK_SCAN_MAX = 32,
};

/* Security and PHY values match the public tabos_wifi_security_t and
 * TABOS_WIFI_PHY_* values. */
typedef struct {
        char ssid[33];
        uint8_t bssid[6];
        uint32_t channel;
        int32_t signal_dbm;
        uint32_t security;
        uint32_t phy;
} network_access_point_t;

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
uint64_t network_service_next_deadline(void);
void network_service_shutdown(void);
bool network_service_connect(const char* ssid, const char* password, bool automatic);
bool network_service_disconnect(void);
bool network_service_status(network_status_t* status);
network_operation_result_t network_service_resolve(const char* hostname, uint32_t family, network_address_t* address);
network_operation_result_t network_service_echo(const network_address_t* address, uint16_t sequence,
                                                uint16_t payload_bytes, uint32_t timeout_ms,
                                                network_echo_result_t* result);
/* Scans nearby access points without changing connection state. Returns up to
 * capacity records, strongest signal first; capacity above NETWORK_SCAN_MAX is
 * clamped. The call blocks for the scan duration and never holds the service
 * operation lock, so runtime updates continue while the radio scans. */
network_operation_result_t network_service_scan(network_access_point_t* access_points, uint32_t capacity,
                                                uint32_t* count);
typedef enum {
    NETWORK_PROFILE_OK = 0,
    NETWORK_PROFILE_NOT_CONNECTED,
    NETWORK_PROFILE_UNAVAILABLE,
    NETWORK_PROFILE_INVALID,
    NETWORK_PROFILE_IO,
} network_profile_result_t;

/* Profile operations serialize their read-modify-write of T:/etc/wifi.conf and
 * run on the caller's task; none of them disconnects or starts a connection.
 * save_current persists the in-memory credentials of the current connection and
 * only succeeds while online, so a failed connection never replaces a saved one. */
network_profile_result_t network_service_save_current(void);
network_profile_result_t network_service_forget(void);
network_profile_result_t network_service_set_auto_connect(bool enabled);
const char* network_state_name(network_state_t state);

#endif
