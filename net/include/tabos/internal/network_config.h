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
    /* Valid file without a saved network. name and auto_connect are filled and
     * ssid/password are empty; written by forgetting the saved profile. */
    NETWORK_CONFIG_NO_PROFILE,
} network_config_result_t;

network_config_result_t network_config_parse(const char* text, size_t length, network_config_t* config);
network_config_result_t network_config_load(network_config_t* config);
/* An empty ssid with an empty password writes a [wifi] section without a saved
 * network, preserving name, auto_connect, comments, and unknown content. */
network_config_result_t network_config_format_update(const char* existing, size_t existing_length,
                                                     const network_config_t* config, char* output,
                                                     size_t output_capacity, size_t* output_length);
/* Atomically replaces T:/etc/wifi.conf. Callers that read, modify, and save must
 * serialize that sequence themselves; the network service does so for profiles. */
network_config_result_t network_config_save(const network_config_t* config);
const char* network_config_result_name(network_config_result_t result);

#endif
