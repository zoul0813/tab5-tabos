#include <tabos/internal/elf_api.h>
#include <tabos/network.h>
#include <tabos/posix_compat.h>
#include <tabos/wait.h>

#include <errno.h>
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
    if (items == NULL || count != 1U || items[0].socket != 3 || timeout_ms != 25U) {
        return -TABOS_EINVAL;
    }
    items[0].returned_events = TABOS_WAIT_READABLE;
    return 1;
}

int main(void)
{
    tabos_network_status_t status;
    if (tabos_network_get_status(NULL) == 0 || errno != EINVAL || tabos_network_get_status(&status) == 0 ||
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
        .socket_wait           = wait_socket,
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
    tabos_wait_item_t wait_item = {.socket = socket, .events = TABOS_WAIT_READABLE};
    if (socket != 3 || tabos_socket_send(socket, "hi", 2U) != 2 ||
        tabos_socket_receive(socket, buffer, sizeof(buffer)) != 2 || memcmp(buffer, "ok", 2U) != 0 ||
        tabos_wait_set(&wait_item, 1U, 25U) != 1 || wait_item.returned_events != TABOS_WAIT_READABLE ||
        tabos_socket_close(socket) != 0) {
        return 1;
    }
    return 0;
}
