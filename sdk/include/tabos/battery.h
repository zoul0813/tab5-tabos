#ifndef TABOS_BATTERY_H
#define TABOS_BATTERY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TABOS_BATTERY_STATE_UNKNOWN,
    TABOS_BATTERY_STATE_CHARGING,
    TABOS_BATTERY_STATE_DISCHARGING,
    TABOS_BATTERY_STATE_FULL,
    TABOS_BATTERY_STATE_NOT_PRESENT
} tabos_battery_state_t;

enum {
    TABOS_BATTERY_VALID_STATE                 = UINT32_C(1) << 0U,
    TABOS_BATTERY_VALID_SOURCE                = UINT32_C(1) << 1U,
    TABOS_BATTERY_VALID_PERCENTAGE            = UINT32_C(1) << 2U,
    TABOS_BATTERY_VALID_VOLTAGE               = UINT32_C(1) << 3U,
    TABOS_BATTERY_VALID_CURRENT               = UINT32_C(1) << 4U,
    TABOS_BATTERY_VALID_POWER                 = UINT32_C(1) << 5U,
    TABOS_BATTERY_VALID_CHARGING_CONTROL      = UINT32_C(1) << 6U,
    TABOS_BATTERY_VALID_FAST_CHARGING_CONTROL = UINT32_C(1) << 7U,
};

typedef struct {
        bool available;
        bool external_power_present;
        bool charging_enabled;
        bool fast_charging_enabled;
        uint32_t valid;
        tabos_battery_state_t state;
        uint32_t percentage;
        uint32_t voltage_mv;
        int32_t current_ma;
        int32_t power_mw;
} tabos_battery_status_t;

int tabos_battery_get_status(tabos_battery_status_t* status);
int tabos_battery_set_charging(bool enabled);
int tabos_battery_set_fast_charging(bool enabled);
const char* tabos_battery_state_name(tabos_battery_state_t state);

#endif
