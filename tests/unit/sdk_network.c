#include <tabos/internal/elf_api.h>
#include <tabos/network.h>
#include <tabos/posix_compat.h>

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
    return 0;
}
