#include <tester/test.h>

#include <tabos/battery.h>
#include <tabos/device.h>

#include <stdint.h>

void tester_test_battery(tester_context_t* context)
{
    tabos_device_info_t device;
    tester_expect(context,
                  tabos_device_find(TABOS_DEVICE_NAME_BATTERY, &device) == 0 &&
                      device.device_class == TABOS_DEVICE_CLASS_BATTERY && device.state == TABOS_DEVICE_READY &&
                      (device.features & TABOS_DEVICE_FEATURE_BATTERY_TELEMETRY) != 0U &&
                      (device.features & TABOS_DEVICE_FEATURE_BATTERY_CHARGE_CONTROL) != 0U,
                  "battery0 reports ready telemetry and control features");

    tabos_battery_status_t status;
    const bool status_ok = tabos_battery_get_status(&status) == 0 && status.available;
    tester_expect(context, status_ok, "battery telemetry is available");
    if (!status_ok) {
        return;
    }

    const uint32_t telemetry = TABOS_BATTERY_VALID_PERCENTAGE | TABOS_BATTERY_VALID_VOLTAGE |
                               TABOS_BATTERY_VALID_CURRENT | TABOS_BATTERY_VALID_POWER;
    tester_expect(context, (status.valid & telemetry) == telemetry && status.percentage <= 100U,
                  "battery telemetry validity and percentage are plausible");
    const int32_t expected_power = (int32_t) ((int64_t) status.voltage_mv * status.current_ma / 1000);
    const int32_t power_delta    = status.power_mw - expected_power;
    tester_expect(context, power_delta >= -10 && power_delta <= 10, "battery voltage, signed current, and power agree");
    tester_expect(context,
                  (status.valid & TABOS_BATTERY_VALID_STATE) == 0U || status.state <= TABOS_BATTERY_STATE_NOT_PRESENT,
                  "battery charge state is valid or explicitly unknown");

    const bool normal_control_valid = (status.valid & TABOS_BATTERY_VALID_CHARGING_CONTROL) != 0U;
    const bool fast_control_valid   = (status.valid & TABOS_BATTERY_VALID_FAST_CHARGING_CONTROL) != 0U;
    tester_expect(context, !normal_control_valid || tabos_battery_set_charging(status.charging_enabled) == 0,
                  "battery accepts unchanged charging state");
    tester_expect(context, !fast_control_valid || tabos_battery_set_fast_charging(status.fast_charging_enabled) == 0,
                  "battery accepts unchanged fast-charge state");
}
