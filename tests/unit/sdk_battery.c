#include <tabos/battery.h>
#include <tabos/internal/elf_api.h>

#include <errno.h>

const tabos_elf_api_t* tabos_runtime_api;

static bool charging_enabled = true;
static bool fast_charging_enabled;

static int battery_status(tabos_elf_battery_status_t* status)
{
    *status = (tabos_elf_battery_status_t) {
        .available              = 1U,
        .external_power_present = 1U,
        .charging_enabled       = charging_enabled ? 1U : 0U,
        .fast_charging_enabled  = fast_charging_enabled ? 1U : 0U,
        .valid = TABOS_BATTERY_VALID_STATE | TABOS_BATTERY_VALID_SOURCE | TABOS_BATTERY_VALID_PERCENTAGE |
                 TABOS_BATTERY_VALID_VOLTAGE | TABOS_BATTERY_VALID_CURRENT | TABOS_BATTERY_VALID_POWER |
                 TABOS_BATTERY_VALID_CHARGING_CONTROL | TABOS_BATTERY_VALID_FAST_CHARGING_CONTROL,
        .voltage_mv   = 8000U,
        .current_ma   = -500,
        .power_mw     = -4000,
        .percentage   = 80U,
        .charge_state = TABOS_BATTERY_STATE_CHARGING,
    };
    return 0;
}

static int battery_set_charging(uint32_t enabled)
{
    charging_enabled = enabled != 0U;
    return 0;
}

static int battery_set_fast_charging(uint32_t enabled)
{
    fast_charging_enabled = enabled != 0U;
    return 0;
}

int main(void)
{
    const tabos_elf_api_t api = {
        .abi_version               = TABOS_ELF_API_VERSION,
        .battery_status            = battery_status,
        .battery_set_charging      = battery_set_charging,
        .battery_set_fast_charging = battery_set_fast_charging,
    };
    tabos_runtime_api = &api;

    tabos_battery_status_t status;
    if (tabos_battery_get_status(&status) != 0 || !status.available || !status.external_power_present ||
        status.state != TABOS_BATTERY_STATE_CHARGING || status.voltage_mv != 8000U || status.current_ma != -500 ||
        status.power_mw != -4000 || status.percentage != 80U || (status.valid & TABOS_BATTERY_VALID_SOURCE) == 0U ||
        tabos_battery_set_charging(false) != 0 || tabos_battery_set_fast_charging(true) != 0 || charging_enabled ||
        !fast_charging_enabled || tabos_battery_get_status(NULL) != -1 || errno != EINVAL) {
        return 1;
    }

    tabos_runtime_api = NULL;
    if (tabos_battery_get_status(&status) != -1 || errno != ENOSYS || tabos_battery_set_charging(true) != -1 ||
        errno != ENOSYS || tabos_battery_set_fast_charging(false) != -1 || errno != ENOSYS) {
        return 1;
    }
    return 0;
}
