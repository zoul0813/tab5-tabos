#include <tabos/internal/elf_api.h>
#include <tabos/network.h>
#include <tabos/posix_compat.h>
#include <tabos/wait.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

const tabos_elf_api_t* tabos_runtime_api;

static int provide_status(tabos_elf_network_status_t* status)
{
    *status = (tabos_elf_network_status_t) {
        .state        = TABOS_NETWORK_ONLINE,
        .signal_dbm   = -42,
        .attempts     = 2U,
        .auto_connect = 1U,
        .saved_config = 1U,
    };
    (void) strcpy(status->hostname, "Test-TabOS");
    (void) strcpy(status->ssid, "test-network");
    (void) strcpy(status->ipv4, "192.0.2.1");
    memset(status->saved_ssid, 's', sizeof(status->saved_ssid));
    return 0;
}

static char connected_ssid[40];
static char connected_password[80];
static uint32_t auto_connect_seen = 9U;

static int connect_network(const char* ssid, const char* password)
{
    (void) snprintf(connected_ssid, sizeof(connected_ssid), "%s", ssid);
    (void) snprintf(connected_password, sizeof(connected_password), "%s", password);
    return strcmp(ssid, "busy") == 0 ? -TABOS_EBUSY : 0;
}

static int save_profile(void)
{
    return -TABOS_ENETDOWN;
}

static int forget_profile(void)
{
    return 0;
}

static int set_auto_connect(uint32_t enabled)
{
    auto_connect_seen = enabled;
    return 0;
}

static int reject_operation(void)
{
    return -TABOS_EBUSY;
}

static int resolve_host(const char* hostname, uint32_t family, tabos_elf_network_address_t* address)
{
    if (strcmp(hostname, "localhost") != 0 || family != 4U) {
        return -TABOS_ENOENT;
    }
    address->family = 4U;
    (void) strcpy(address->text, "127.0.0.1");
    return 0;
}

static int echo_host(const tabos_elf_network_address_t* address, uint32_t sequence, uint32_t payload_bytes,
                     uint32_t timeout_ms, tabos_elf_network_echo_result_t* result)
{
    if (address->family != 4U || timeout_ms == 0U) {
        return -TABOS_EINVAL;
    }
    *result = (tabos_elf_network_echo_result_t) {
        .sequence      = sequence,
        .bytes         = payload_bytes,
        .round_trip_ms = 3U,
    };
    return 0;
}

static int open_socket(uint32_t family, uint32_t type)
{
    return family == 4U && type == TABOS_SOCKET_TCP ? 3 : -TABOS_EINVAL;
}

static int close_socket(int socket)
{
    return socket == 3 ? 0 : -TABOS_EBADF;
}

static int send_socket(int socket, const void* data, uint32_t size)
{
    return socket == 3 && data != NULL ? (int) size : -TABOS_EBADF;
}

static int receive_socket(int socket, void* data, uint32_t capacity)
{
    if (socket != 3 || data == NULL || capacity < 2U) {
        return -TABOS_EINVAL;
    }
    memcpy(data, "ok", 2U);
    return 2;
}

static int wait_socket(tabos_elf_wait_item_t* items, uint32_t count, uint32_t timeout_ms)
{
    if (items == NULL || count != 1U || items[0].source != 19 || timeout_ms != 25U) {
        return -TABOS_EINVAL;
    }
    items[0].returned_events = TABOS_WAIT_READABLE;
    return 1;
}

static uint32_t scan_capacity_seen;

static int scan_networks(tabos_elf_wifi_access_point_t* access_points, uint32_t capacity)
{
    scan_capacity_seen = capacity;
    if (capacity == 7U) {
        return -TABOS_EBUSY;
    }
    access_points[0] = (tabos_elf_wifi_access_point_t) {
        .bssid      = {0x02U, 0x00U, 0x5eU, 0x00U, 0x00U, 0x01U},
        .channel    = 6U,
        .signal_dbm = -38,
        .security   = TABOS_WIFI_SECURITY_WPA2,
        .phy        = TABOS_WIFI_PHY_11B | TABOS_WIFI_PHY_11AX,
    };
    memset(access_points[0].ssid, 'n', sizeof(access_points[0].ssid));
    access_points[1]          = (tabos_elf_wifi_access_point_t) {.channel = 11U, .signal_dbm = -80};
    access_points[1].security = 200U;
    return 2;
}

static int scan_test(void)
{
    tabos_wifi_access_point_t found[TABOS_WIFI_SCAN_MAX];
    if (tabos_network_scan(NULL, 4U) == 0 || errno != EINVAL || tabos_network_scan(found, 0U) == 0 ||
        errno != EINVAL) {
        return 1;
    }
    if (tabos_network_scan(found, 4U) != 2 || scan_capacity_seen != 4U || found[0].ssid[32] != '\0' ||
        strlen(found[0].ssid) != 32U || found[0].bssid[5] != 1U || found[0].channel != 6U ||
        found[0].signal_dbm != -38 || found[0].security != TABOS_WIFI_SECURITY_WPA2 ||
        found[0].phy != (TABOS_WIFI_PHY_11B | TABOS_WIFI_PHY_11AX) || found[1].ssid[0] != '\0' ||
        found[1].security != TABOS_WIFI_SECURITY_UNKNOWN) {
        return 1;
    }
    if (tabos_network_scan(found, 500U) != 2 || scan_capacity_seen != TABOS_WIFI_SCAN_MAX) {
        return 1;
    }
    if (tabos_network_scan(found, 7U) != -1 || errno != TABOS_EBUSY) {
        return 1;
    }
    if (strcmp(tabos_wifi_security_name(TABOS_WIFI_SECURITY_OPEN), "open") != 0 ||
        strcmp(tabos_wifi_security_name(TABOS_WIFI_SECURITY_WPA2_WPA3), "WPA2/WPA3") != 0 ||
        strcmp(tabos_wifi_security_name((tabos_wifi_security_t) 99), "unknown") != 0) {
        return 1;
    }
    return 0;
}

static int socket_wait_source(int socket)
{
    return socket == 3 ? 19 : -TABOS_EBADF;
}

int main(void)
{
    tabos_network_status_t status;
    if (tabos_network_get_status(NULL) == 0 || errno != EINVAL || tabos_network_get_status(&status) == 0 ||
        errno != ENOSYS) {
        return 1;
    }
    tabos_wifi_access_point_t unavailable[1];
    if (tabos_network_scan(unavailable, 1U) == 0 || errno != ENOSYS || tabos_network_connect("x", "") == 0 ||
        errno != ENOSYS || tabos_network_save_profile() == 0 || errno != ENOSYS ||
        tabos_network_forget_profile() == 0 || errno != ENOSYS || tabos_network_set_auto_connect(true) == 0 ||
        errno != ENOSYS) {
        return 1;
    }
    const tabos_elf_api_t api = {
        .network_status        = provide_status,
        .network_connect_saved = reject_operation,
        .network_disconnect    = reject_operation,
        .network_resolve       = resolve_host,
        .network_echo          = echo_host,
        .socket_open           = open_socket,
        .socket_close          = close_socket,
        .socket_send           = send_socket,
        .socket_receive        = receive_socket,
        .socket_wait_source    = socket_wait_source,
        .wait                  = wait_socket,
        .network_scan          = scan_networks,
        .network_connect       = connect_network,
        .network_save_profile  = save_profile,
        .network_forget_profile = forget_profile,
        .network_set_auto_connect = set_auto_connect,
    };
    tabos_runtime_api = &api;
    if (tabos_network_get_status(&status) != 0 || status.state != TABOS_NETWORK_ONLINE ||
        strcmp(status.hostname, "Test-TabOS") != 0 || strcmp(status.ssid, "test-network") != 0 ||
        strcmp(status.ipv4, "192.0.2.1") != 0 || status.signal_dbm != -42 || status.attempts != 2U ||
        !status.auto_connect || !status.saved_config) {
        return 1;
    }
    if (tabos_network_connect_saved() == 0 || errno != TABOS_EBUSY || tabos_network_disconnect() == 0 ||
        errno != TABOS_EBUSY || strcmp(tabos_network_state_name(TABOS_NETWORK_FAILED), "failed") != 0) {
        return 1;
    }
    tabos_network_address_t address;
    tabos_network_echo_result_t echo;
    if (tabos_network_resolve("localhost", TABOS_NETWORK_FAMILY_IPV4, &address) != 0 ||
        address.family != TABOS_NETWORK_FAMILY_IPV4 || strcmp(address.text, "127.0.0.1") != 0 ||
        tabos_network_echo(&address, 9U, 56U, 1000U, &echo) != 0 || echo.sequence != 9U || echo.bytes != 56U ||
        echo.round_trip_ms != 3U) {
        return 1;
    }
    char buffer[4]              = {0};
    const tabos_socket_t socket = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_TCP);
    tabos_wait_item_t wait_item = {
        .source = tabos_socket_wait_source(socket),
        .events = TABOS_WAIT_READABLE,
    };
    if (socket != 3 || tabos_socket_send(socket, "hi", 2U) != 2 ||
        tabos_socket_receive(socket, buffer, sizeof(buffer)) != 2 || memcmp(buffer, "ok", 2U) != 0 ||
        wait_item.source != 19 || tabos_wait(&wait_item, 1U, 25U) != 1 ||
        wait_item.returned_events != TABOS_WAIT_READABLE || tabos_socket_close(socket) != 0) {
        return 1;
    }
    if (scan_test() != 0) {
        return 1;
    }
    if (status.saved_ssid[sizeof(status.saved_ssid) - 1U] != '\0' || strlen(status.saved_ssid) != 32U) {
        return 1;
    }
    char long_ssid[34];
    memset(long_ssid, 'a', sizeof(long_ssid) - 1U);
    long_ssid[sizeof(long_ssid) - 1U] = '\0';
    char long_password[66];
    memset(long_password, 'p', sizeof(long_password) - 1U);
    long_password[sizeof(long_password) - 1U] = '\0';
    if (tabos_network_connect(NULL, "") == 0 || errno != EINVAL || tabos_network_connect("x", NULL) == 0 ||
        errno != EINVAL || tabos_network_connect("", "") == 0 || errno != EINVAL ||
        tabos_network_connect(long_ssid, "") == 0 || errno != EINVAL || tabos_network_connect("x", long_password) == 0 ||
        errno != EINVAL || connected_ssid[0] != '\0') {
        return 1;
    }
    long_ssid[32]     = '\0';
    long_password[64] = '\0';
    if (tabos_network_connect(long_ssid, long_password) != 0 || strcmp(connected_ssid, long_ssid) != 0 ||
        strcmp(connected_password, long_password) != 0 || tabos_network_connect("busy", "pw") == 0 ||
        errno != TABOS_EBUSY) {
        return 1;
    }
    if (tabos_network_save_profile() == 0 || errno != TABOS_ENETDOWN || tabos_network_forget_profile() != 0 ||
        tabos_network_set_auto_connect(false) != 0 || auto_connect_seen != 0U ||
        tabos_network_set_auto_connect(true) != 0 || auto_connect_seen != 1U) {
        return 1;
    }
    return 0;
}
