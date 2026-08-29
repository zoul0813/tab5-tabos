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

const char* tabos_network_state_name(tabos_network_state_t state)
{
    static const char* const names[] = {
        "offline", "starting", "scanning", "connecting", "online", "disconnecting", "failed",
    };
    return (unsigned int) state < sizeof(names) / sizeof(names[0]) ? names[state] : "unknown";
}
