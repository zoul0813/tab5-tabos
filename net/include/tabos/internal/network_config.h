#ifndef TABOS_INTERNAL_NETWORK_CONFIG_H
#define TABOS_INTERNAL_NETWORK_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

enum {
    NETWORK_CONFIG_SSID_MAX     = 32,
    NETWORK_CONFIG_PASSWORD_MAX = 64,
    NETWORK_CONFIG_NAME_MAX     = 32,
    NETWORK_CONFIG_FILE_MAX     = 4096,
};

#define NETWORK_CONFIG_DEFAULT_NAME "TabOS"

typedef struct {
        char ssid[NETWORK_CONFIG_SSID_MAX + 1U];
        char password[NETWORK_CONFIG_PASSWORD_MAX + 1U];
        char name[NETWORK_CONFIG_NAME_MAX + 1U];
        bool auto_connect;
} network_config_t;

typedef enum {
    NETWORK_CONFIG_OK = 0,
    NETWORK_CONFIG_NOT_FOUND,
    NETWORK_CONFIG_UNAVAILABLE,
    NETWORK_CONFIG_INVALID,
    NETWORK_CONFIG_TOO_LARGE,
    NETWORK_CONFIG_IO_ERROR,
} network_config_result_t;

network_config_result_t network_config_parse(const char* text, size_t length, network_config_t* config);
network_config_result_t network_config_load(network_config_t* config);
network_config_result_t network_config_format_update(const char* existing, size_t existing_length,
                                                     const network_config_t* config, char* output,
                                                     size_t output_capacity, size_t* output_length);
network_config_result_t network_config_save(const network_config_t* config);
const char* network_config_result_name(network_config_result_t result);

#endif
