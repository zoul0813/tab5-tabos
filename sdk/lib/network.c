#include <tabos/internal/elf_api.h>
#include <tabos/network.h>

#include <errno.h>
#include <string.h>

extern const tabos_elf_api_t* tabos_runtime_api;

static int api_result(int result)
{
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return result;
}

int tabos_network_get_status(tabos_network_status_t* status)
{
    if (status == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->network_status == NULL) {
        errno = ENOSYS;
        return -1;
    }
    tabos_elf_network_status_t source;
    const int result = tabos_runtime_api->network_status(&source);
    if (result < 0) {
        return api_result(result);
    }
    memset(status, 0, sizeof(*status));
    status->state        = (tabos_network_state_t) source.state;
    status->signal_dbm   = source.signal_dbm;
    status->attempts     = source.attempts;
    status->auto_connect = source.auto_connect != 0U;
    status->saved_config = source.saved_config != 0U;
    memcpy(status->hostname, source.hostname, sizeof(status->hostname));
    memcpy(status->ssid, source.ssid, sizeof(status->ssid));
    memcpy(status->ipv4, source.ipv4, sizeof(status->ipv4));
    memcpy(status->last_failure, source.last_failure, sizeof(status->last_failure));
    return 0;
}

int tabos_network_connect_saved(void)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->network_connect_saved == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->network_connect_saved());
}

int tabos_network_disconnect(void)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->network_disconnect == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->network_disconnect());
}

int tabos_network_resolve(const char* hostname, tabos_network_family_t family, tabos_network_address_t* address)
{
    if (hostname == NULL || address == NULL || hostname[0] == '\0' ||
        (family != TABOS_NETWORK_FAMILY_ANY && family != TABOS_NETWORK_FAMILY_IPV4 &&
         family != TABOS_NETWORK_FAMILY_IPV6)) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->network_resolve == NULL) {
        errno = ENOSYS;
        return -1;
    }
    tabos_elf_network_address_t resolved;
    const int operation = tabos_runtime_api->network_resolve(hostname, (uint32_t) family, &resolved);
    if (operation < 0) {
        return api_result(operation);
    }
    memset(address, 0, sizeof(*address));
    address->family = (tabos_network_family_t) resolved.family;
    memcpy(address->text, resolved.text, sizeof(address->text));
    return 0;
}

int tabos_network_echo(const tabos_network_address_t* address, uint16_t sequence, uint16_t payload_bytes,
                       uint32_t timeout_ms, tabos_network_echo_result_t* result)
{
    if (address == NULL || result == NULL ||
        (address->family != TABOS_NETWORK_FAMILY_IPV4 && address->family != TABOS_NETWORK_FAMILY_IPV6) ||
        address->text[0] == '\0' || memchr(address->text, '\0', sizeof(address->text)) == NULL ||
        payload_bytes > 1024U || timeout_ms == 0U || timeout_ms > 60000U) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->network_echo == NULL) {
        errno = ENOSYS;
        return -1;
    }
    tabos_elf_network_address_t target = {.family = (uint32_t) address->family};
    memcpy(target.text, address->text, sizeof(target.text));
    tabos_elf_network_echo_result_t echoed;
    const int operation = tabos_runtime_api->network_echo(&target, sequence, payload_bytes, timeout_ms, &echoed);
    if (operation < 0) {
        return api_result(operation);
    }
    result->sequence      = echoed.sequence;
    result->bytes         = echoed.bytes;
    result->round_trip_ms = echoed.round_trip_ms;
    return 0;
}

static bool valid_endpoint(const tabos_socket_endpoint_t* endpoint)
{
    return endpoint != NULL &&
           (endpoint->address.family == TABOS_NETWORK_FAMILY_IPV4 ||
            endpoint->address.family == TABOS_NETWORK_FAMILY_IPV6) &&
           endpoint->address.text[0] != '\0' &&
           memchr(endpoint->address.text, '\0', sizeof(endpoint->address.text)) != NULL;
}

static void endpoint_to_elf(const tabos_socket_endpoint_t* endpoint, tabos_elf_socket_endpoint_t* target)
{
    memset(target, 0, sizeof(*target));
    target->address.family = (uint32_t) endpoint->address.family;
    memcpy(target->address.text, endpoint->address.text, sizeof(target->address.text));
    target->port = endpoint->port;
}

static void endpoint_from_elf(const tabos_elf_socket_endpoint_t* source, tabos_socket_endpoint_t* endpoint)
{
    memset(endpoint, 0, sizeof(*endpoint));
    endpoint->address.family = (tabos_network_family_t) source->address.family;
    memcpy(endpoint->address.text, source->address.text, sizeof(endpoint->address.text));
    endpoint->port = (uint16_t) source->port;
}

tabos_socket_t tabos_socket_open(tabos_network_family_t family, tabos_socket_type_t type)
{
    if ((family != TABOS_NETWORK_FAMILY_IPV4 && family != TABOS_NETWORK_FAMILY_IPV6) ||
        (type != TABOS_SOCKET_TCP && type != TABOS_SOCKET_UDP)) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->socket_open == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->socket_open((uint32_t) family, (uint32_t) type));
}

int tabos_socket_close(tabos_socket_t socket)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->socket_close == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->socket_close(socket));
}

int tabos_socket_bind(tabos_socket_t socket, const tabos_socket_endpoint_t* endpoint)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->socket_bind == NULL || !valid_endpoint(endpoint)) {
        errno = tabos_runtime_api == NULL || tabos_runtime_api->socket_bind == NULL ? ENOSYS : EINVAL;
        return -1;
    }
    tabos_elf_socket_endpoint_t target;
    endpoint_to_elf(endpoint, &target);
    return api_result(tabos_runtime_api->socket_bind(socket, &target));
}

int tabos_socket_get_local_endpoint(tabos_socket_t socket, tabos_socket_endpoint_t* endpoint)
{
    if (endpoint == NULL || tabos_runtime_api == NULL || tabos_runtime_api->socket_get_local_endpoint == NULL) {
        errno = endpoint == NULL ? EINVAL : ENOSYS;
        return -1;
    }
    tabos_elf_socket_endpoint_t source;
    const int result = tabos_runtime_api->socket_get_local_endpoint(socket, &source);
    if (result < 0) {
        return api_result(result);
    }
    endpoint_from_elf(&source, endpoint);
    return 0;
}

int tabos_socket_listen(tabos_socket_t socket, uint16_t backlog)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->socket_listen == NULL || backlog == 0U) {
        errno = tabos_runtime_api == NULL || tabos_runtime_api->socket_listen == NULL ? ENOSYS : EINVAL;
        return -1;
    }
    return api_result(tabos_runtime_api->socket_listen(socket, backlog));
}

tabos_socket_t tabos_socket_accept(tabos_socket_t socket, tabos_socket_endpoint_t* peer)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->socket_accept == NULL) {
        errno = ENOSYS;
        return -1;
    }
    tabos_elf_socket_endpoint_t source;
    const int accepted = tabos_runtime_api->socket_accept(socket, peer != NULL ? &source : NULL);
    if (accepted < 0) {
        return api_result(accepted);
    }
    if (peer != NULL) {
        endpoint_from_elf(&source, peer);
    }
    return accepted;
}

int tabos_socket_connect(tabos_socket_t socket, const tabos_socket_endpoint_t* endpoint)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->socket_connect == NULL || !valid_endpoint(endpoint)) {
        errno = tabos_runtime_api == NULL || tabos_runtime_api->socket_connect == NULL ? ENOSYS : EINVAL;
        return -1;
    }
    tabos_elf_socket_endpoint_t target;
    endpoint_to_elf(endpoint, &target);
    return api_result(tabos_runtime_api->socket_connect(socket, &target));
}

int tabos_socket_set_nonblocking(tabos_socket_t socket, bool enabled)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->socket_set_nonblocking == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->socket_set_nonblocking(socket, enabled ? 1U : 0U));
}

int tabos_socket_shutdown(tabos_socket_t socket, tabos_socket_shutdown_t direction)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->socket_shutdown == NULL ||
        direction > TABOS_SOCKET_SHUTDOWN_BOTH) {
        errno = direction > TABOS_SOCKET_SHUTDOWN_BOTH ? EINVAL : ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->socket_shutdown(socket, (uint32_t) direction));
}

int tabos_socket_send(tabos_socket_t socket, const void* data, uint32_t size)
{
    if (data == NULL || size == 0U || size > TABOS_NETWORK_IO_MAX || tabos_runtime_api == NULL ||
        tabos_runtime_api->socket_send == NULL) {
        errno = tabos_runtime_api == NULL || tabos_runtime_api->socket_send == NULL ? ENOSYS : EINVAL;
        return -1;
    }
    return api_result(tabos_runtime_api->socket_send(socket, data, size));
}

int tabos_socket_receive(tabos_socket_t socket, void* data, uint32_t capacity)
{
    if (data == NULL || capacity == 0U || capacity > TABOS_NETWORK_IO_MAX || tabos_runtime_api == NULL ||
        tabos_runtime_api->socket_receive == NULL) {
        errno = tabos_runtime_api == NULL || tabos_runtime_api->socket_receive == NULL ? ENOSYS : EINVAL;
        return -1;
    }
    return api_result(tabos_runtime_api->socket_receive(socket, data, capacity));
}

int tabos_socket_send_to(tabos_socket_t socket, const void* data, uint32_t size,
                         const tabos_socket_endpoint_t* endpoint)
{
    if (!valid_endpoint(endpoint) || data == NULL || size == 0U || size > TABOS_NETWORK_IO_MAX ||
        tabos_runtime_api == NULL || tabos_runtime_api->socket_send_to == NULL) {
        errno = tabos_runtime_api == NULL || tabos_runtime_api->socket_send_to == NULL ? ENOSYS : EINVAL;
        return -1;
    }
    tabos_elf_socket_endpoint_t target;
    endpoint_to_elf(endpoint, &target);
    return api_result(tabos_runtime_api->socket_send_to(socket, data, size, &target));
}

int tabos_socket_receive_from(tabos_socket_t socket, void* data, uint32_t capacity, tabos_socket_endpoint_t* peer)
{
    if (data == NULL || capacity == 0U || capacity > TABOS_NETWORK_IO_MAX || tabos_runtime_api == NULL ||
        tabos_runtime_api->socket_receive_from == NULL) {
        errno = tabos_runtime_api == NULL || tabos_runtime_api->socket_receive_from == NULL ? ENOSYS : EINVAL;
        return -1;
    }
    tabos_elf_socket_endpoint_t source;
    const int received = tabos_runtime_api->socket_receive_from(socket, data, capacity, peer != NULL ? &source : NULL);
    if (received < 0) {
        return api_result(received);
    }
    if (peer != NULL) {
        endpoint_from_elf(&source, peer);
    }
    return received;
}

const char* tabos_network_state_name(tabos_network_state_t state)
{
    static const char* const names[] = {
        "offline", "starting", "scanning", "connecting", "online", "disconnecting", "failed",
    };
    return (unsigned int) state < sizeof(names) / sizeof(names[0]) ? names[state] : "unknown";
}
