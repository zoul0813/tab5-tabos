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

typedef enum {
    TABOS_NETWORK_FAMILY_ANY  = 0,
    TABOS_NETWORK_FAMILY_IPV4 = 4,
    TABOS_NETWORK_FAMILY_IPV6 = 6,
} tabos_network_family_t;

typedef struct {
        tabos_network_family_t family;
        char text[46];
} tabos_network_address_t;

typedef struct {
        uint32_t sequence;
        uint32_t bytes;
        uint32_t round_trip_ms;
} tabos_network_echo_result_t;

enum {
    TABOS_NETWORK_IO_MAX = 1024,
};

typedef int32_t tabos_socket_t;

typedef enum {
    TABOS_SOCKET_TCP = 1,
    TABOS_SOCKET_UDP = 2,
} tabos_socket_type_t;

typedef enum {
    TABOS_SOCKET_SHUTDOWN_READ  = 0,
    TABOS_SOCKET_SHUTDOWN_WRITE = 1,
    TABOS_SOCKET_SHUTDOWN_BOTH  = 2,
} tabos_socket_shutdown_t;

typedef struct {
        tabos_network_address_t address;
        uint16_t port;
} tabos_socket_endpoint_t;

int tabos_network_get_status(tabos_network_status_t* status);
int tabos_network_connect_saved(void);
int tabos_network_disconnect(void);
int tabos_network_resolve(const char* hostname, tabos_network_family_t family, tabos_network_address_t* address);
int tabos_network_echo(const tabos_network_address_t* address, uint16_t sequence, uint16_t payload_bytes,
                       uint32_t timeout_ms, tabos_network_echo_result_t* result);
tabos_socket_t tabos_socket_open(tabos_network_family_t family, tabos_socket_type_t type);
int tabos_socket_close(tabos_socket_t socket);
int tabos_socket_bind(tabos_socket_t socket, const tabos_socket_endpoint_t* endpoint);
int tabos_socket_get_local_endpoint(tabos_socket_t socket, tabos_socket_endpoint_t* endpoint);
int tabos_socket_listen(tabos_socket_t socket, uint16_t backlog);
tabos_socket_t tabos_socket_accept(tabos_socket_t socket, tabos_socket_endpoint_t* peer);
int tabos_socket_connect(tabos_socket_t socket, const tabos_socket_endpoint_t* endpoint);
int tabos_socket_set_nonblocking(tabos_socket_t socket, bool enabled);
int tabos_socket_shutdown(tabos_socket_t socket, tabos_socket_shutdown_t direction);
int tabos_socket_send(tabos_socket_t socket, const void* data, uint32_t size);
int tabos_socket_receive(tabos_socket_t socket, void* data, uint32_t capacity);
int tabos_socket_send_to(tabos_socket_t socket, const void* data, uint32_t size,
                         const tabos_socket_endpoint_t* endpoint);
int tabos_socket_receive_from(tabos_socket_t socket, void* data, uint32_t capacity, tabos_socket_endpoint_t* peer);
const char* tabos_network_state_name(tabos_network_state_t state);

#endif
