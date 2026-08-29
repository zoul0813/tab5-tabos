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
    printf("State: %s\n", tabos_battery_state_name(status.state));
    printf("Charge: %s\n", status.charging_enabled ? "enabled" : "disabled");
    printf("Fast charge: %s\n", status.fast_charging_enabled ? "enabled" : "disabled");
    printf("Level: %lu%% (estimated)\n", (unsigned long) status.percentage);
    printf("Voltage: %lu mV\n", (unsigned long) status.voltage_mv);
    printf("Current: %ld mA\n", (long) status.current_ma);
    printf("Power: %ld mW\n", (long) status.power_mw);
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
