#include <tabos/battery.h>
#include <tabos/internal/elf_api.h>

#include <errno.h>
#include <string.h>

extern const tabos_elf_api_t* tabos_runtime_api;

int tabos_battery_get_status(tabos_battery_status_t* status)
{
    if (status == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->battery_status == NULL) {
        errno = ENOSYS;
        return -1;
    }
    tabos_elf_battery_status_t source;
    const int result = tabos_runtime_api->battery_status(&source);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    memset(status, 0, sizeof(*status));
    status->available = source.available != 0U;
    status->charging_enabled = source.charging_enabled != 0U;
    status->fast_charging_enabled = source.fast_charging_enabled != 0U;
    status->state = (tabos_battery_state_t) source.charge_state;
    status->percentage = source.percentage;
    status->voltage_mv = source.voltage_mv;
    status->current_ma = source.current_ma;
    status->power_mw = source.power_mw;
    return 0;
}

int tabos_battery_set_fast_charging(bool enabled)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->battery_set_fast_charging == NULL) {
        errno = ENOSYS;
        return -1;
    }
    const int result = tabos_runtime_api->battery_set_fast_charging(enabled ? 1U : 0U);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return 0;
}

int tabos_battery_set_charging(bool enabled)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->battery_set_charging == NULL) {
        errno = ENOSYS;
        return -1;
    }
    const int result = tabos_runtime_api->battery_set_charging(enabled ? 1U : 0U);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return 0;
}

const char* tabos_battery_state_name(tabos_battery_state_t state)
{
    switch (state) {
    case TABOS_BATTERY_STATE_CHARGING:
        return "charging";
    case TABOS_BATTERY_STATE_DISCHARGING:
        return "discharging";
    case TABOS_BATTERY_STATE_FULL:
        return "full";
    case TABOS_BATTERY_STATE_NOT_PRESENT:
        return "not present";
    default:
        return "unknown";
    }
}
