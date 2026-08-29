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
    const int operation =
        tabos_runtime_api->network_echo(&target, sequence, payload_bytes, timeout_ms, &echoed);
    if (operation < 0) {
        return api_result(operation);
    }
    result->sequence      = echoed.sequence;
    result->bytes         = echoed.bytes;
    result->round_trip_ms = echoed.round_trip_ms;
    return 0;
}

const char* tabos_network_state_name(tabos_network_state_t state)
{
    static const char* const names[] = {
        "offline", "starting", "scanning", "connecting", "online", "disconnecting", "failed",
    };
    return (unsigned int) state < sizeof(names) / sizeof(names[0]) ? names[state] : "unknown";
}
