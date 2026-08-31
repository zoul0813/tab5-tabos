#include <tabos/battery.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

static void usage(FILE* stream)
{
    fprintf(stream, "Usage: battery [status|charge on|charge off|fast on|fast off]\n");
}

static int show_status(void)
{
    tabos_battery_status_t status;
    if (tabos_battery_get_status(&status) != 0) {
        fprintf(stderr, "battery: cannot read status: %s\n", strerror(errno));
        return 1;
    }
    if (!status.available) {
        puts("Battery: unavailable");
        return 0;
    }
    printf("Source: %s\n", (status.valid & TABOS_BATTERY_VALID_SOURCE) == 0U ? "unknown" :
                           status.external_power_present                     ? "external" :
                                                                               "battery");
    printf("State: %s\n",
           (status.valid & TABOS_BATTERY_VALID_STATE) != 0U ? tabos_battery_state_name(status.state) : "unknown");
    printf("Charge: %s\n", (status.valid & TABOS_BATTERY_VALID_CHARGING_CONTROL) == 0U ? "unknown" :
                           status.charging_enabled                                     ? "enabled" :
                                                                                         "disabled");
    printf("Fast charge: %s\n", (status.valid & TABOS_BATTERY_VALID_FAST_CHARGING_CONTROL) == 0U ? "unknown" :
                                status.fast_charging_enabled                                     ? "enabled" :
                                                                                                   "disabled");
    if ((status.valid & TABOS_BATTERY_VALID_PERCENTAGE) != 0U) {
        printf("Level: %lu%% (estimated)\n", (unsigned long) status.percentage);
    } else {
        puts("Level: unknown");
    }
    if ((status.valid & TABOS_BATTERY_VALID_VOLTAGE) != 0U) {
        printf("Voltage: %lu mV\n", (unsigned long) status.voltage_mv);
    } else {
        puts("Voltage: unknown");
    }
    if ((status.valid & TABOS_BATTERY_VALID_CURRENT) != 0U) {
        printf("Current: %+ld mA\n", (long) status.current_ma);
    } else {
        puts("Current: unknown");
    }
    if ((status.valid & TABOS_BATTERY_VALID_POWER) != 0U) {
        printf("Power: %+ld mW\n", (long) status.power_mw);
    } else {
        puts("Power: unknown");
    }
    return 0;
}

static int set_state(const char* name, bool enabled)
{
    int result;
    if (strcmp(name, "charge") == 0) {
        result = tabos_battery_set_charging(enabled);
    } else {
        result = tabos_battery_set_fast_charging(enabled);
    }
    if (result != 0) {
        fprintf(stderr, "battery: cannot set %s: %s\n", name, strerror(errno));
        return 1;
    }
    printf("%s %s.\n", name, enabled ? "enabled" : "disabled");
    return 0;
}

int main(int argc, char** argv)
{
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "status") == 0)) {
        return show_status();
    }
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        usage(stdout);
        return 0;
    }
    if (argc == 3 && (strcmp(argv[1], "charge") == 0 || strcmp(argv[1], "fast") == 0) &&
        (strcmp(argv[2], "on") == 0 || strcmp(argv[2], "off") == 0)) {
        return set_state(argv[1], strcmp(argv[2], "on") == 0);
    }
    usage(stderr);
    return 1;
}
