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
        /* SSID of the saved profile, or empty; ssid is the current target. */
        char saved_ssid[33];
} tabos_network_status_t;

enum {
    TABOS_NETWORK_SSID_MAX     = 32,
    TABOS_NETWORK_PASSWORD_MAX = 64,
};

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
    TABOS_SOCKET_MAX     = 8,
    TABOS_WIFI_SCAN_MAX  = 32,
};

typedef enum {
    TABOS_WIFI_SECURITY_OPEN = 0,
    TABOS_WIFI_SECURITY_WEP,
    TABOS_WIFI_SECURITY_WPA,
    TABOS_WIFI_SECURITY_WPA2,
    TABOS_WIFI_SECURITY_WPA_WPA2,
    TABOS_WIFI_SECURITY_WPA3,
    TABOS_WIFI_SECURITY_WPA2_WPA3,
    TABOS_WIFI_SECURITY_ENTERPRISE,
    TABOS_WIFI_SECURITY_OWE,
    TABOS_WIFI_SECURITY_UNKNOWN,
} tabos_wifi_security_t;

/* Bits in tabos_wifi_access_point_t.phy. */
enum {
    TABOS_WIFI_PHY_11B  = 1U << 0U,
    TABOS_WIFI_PHY_11G  = 1U << 1U,
    TABOS_WIFI_PHY_11N  = 1U << 2U,
    TABOS_WIFI_PHY_11AX = 1U << 3U,
};

/* One nearby access point. An empty ssid is a hidden network. */
typedef struct {
        char ssid[33];
        uint8_t bssid[6];
        uint32_t channel;
        int32_t signal_dbm;
        tabos_wifi_security_t security;
        uint32_t phy;
} tabos_wifi_access_point_t;

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
/* Starts connecting to ssid without saving it; an empty password selects an open
 * network. Returns once the attempt starts: poll tabos_network_get_status() for
 * TABOS_NETWORK_ONLINE, or FAILED after the third attempt. The saved profile is
 * untouched. Callers should clear their password buffer afterwards. */
int tabos_network_connect(const char* ssid, const char* password);
/* Saves the current connection as the profile in T:/etc/wifi.conf. Only succeeds
 * while online (ENETDOWN otherwise), so a failed connection never replaces the
 * saved network. The kernel supplies the password; none is passed back in.
 * ENODEV reports missing T: storage and EINVAL a damaged wifi.conf. */
int tabos_network_save_profile(void);
/* Removes the saved network, keeping hostname, autoconnect policy, comments, and
 * unknown settings. Does not disconnect. */
int tabos_network_forget_profile(void);
/* Sets whether the saved network is joined at boot. */
int tabos_network_set_auto_connect(bool enabled);
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
/* Scans nearby 2.4 GHz access points without changing the current connection.
 * Blocks for the scan, usually a few seconds. Stores up to capacity records,
 * strongest signal first; capacity above TABOS_WIFI_SCAN_MAX is clamped.
 * Returns the stored count, or -1 with errno EINVAL, ENETDOWN (radio
 * unavailable), EBUSY (connecting, starting, or another scan), ETIMEDOUT, or EIO. */
int tabos_network_scan(tabos_wifi_access_point_t* access_points, uint32_t capacity);
const char* tabos_network_state_name(tabos_network_state_t state);
const char* tabos_wifi_security_name(tabos_wifi_security_t security);

#endif
