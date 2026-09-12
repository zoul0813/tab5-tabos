#ifndef TABOS_INTERNAL_POWER_CONFIG_H
#define TABOS_INTERNAL_POWER_CONFIG_H

#include <tabos/internal/power.h>

#define POWER_CONFIG_PATH "T:/etc/power.conf"
enum {
    POWER_CONFIG_FILE_MAX = 4096
};

typedef enum {
    POWER_CONFIG_OK = 0,
    POWER_CONFIG_NOT_FOUND,
    POWER_CONFIG_UNAVAILABLE,
    POWER_CONFIG_INVALID,
    POWER_CONFIG_TOO_LARGE,
    POWER_CONFIG_IO_ERROR,
} power_config_result_t;

power_policy_t power_config_defaults(void);
/* Parsing/loading publishes a complete policy only on success. */
power_config_result_t power_config_parse(const char* text, size_t length, power_policy_t* policy);
power_config_result_t power_config_load(power_policy_t* policy);
const char* power_config_result_name(power_config_result_t result);

#endif
