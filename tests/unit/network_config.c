#include <tabos/internal/network_config.h>

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

int main(void)
{
    static const char valid[] = "# TabOS network\n"
                                "version=1\n\n"
                                "[wifi]\n"
                                "ssid=\"cave wifi\"\n"
                                "password=\"rock\\\"and\\\\roll\"\n"
                                "auto_connect=false\n\n"
                                "[dns]\n"
                                "servers=\"future\"\n";
    network_config_t config;
    expect(network_config_parse(valid, sizeof(valid) - 1U, &config) == NETWORK_CONFIG_OK, "valid config parses");
    expect(strcmp(config.ssid, "cave wifi") == 0, "SSID parsed");
    expect(strcmp(config.password, "rock\"and\\roll") == 0, "password escapes parsed");
    expect(!config.auto_connect, "autoconnect false parsed");

    static const char open_network[] = "version=1\n[wifi]\nssid=\"open\"\npassword=\"\"\n";
    expect(network_config_parse(open_network, sizeof(open_network) - 1U, &config) == NETWORK_CONFIG_OK,
           "open network parses");
    expect(config.password[0] == '\0', "open network has empty password");
    expect(config.auto_connect, "autoconnect defaults true");

    static const char bad_version[] = "version=2\n[wifi]\nssid=\"x\"\n";
    expect(network_config_parse(bad_version, sizeof(bad_version) - 1U, &config) == NETWORK_CONFIG_INVALID,
           "future version rejected");
    static const char missing_ssid[] = "version=1\n[wifi]\npassword=\"x\"\n";
    expect(network_config_parse(missing_ssid, sizeof(missing_ssid) - 1U, &config) == NETWORK_CONFIG_INVALID,
           "missing SSID rejected");
    static const char bad_escape[] = "version=1\n[wifi]\nssid=\"bad\\n\"\n";
    expect(network_config_parse(bad_escape, sizeof(bad_escape) - 1U, &config) == NETWORK_CONFIG_INVALID,
           "unknown escape rejected");
    expect(network_config_parse(valid, NETWORK_CONFIG_FILE_MAX + 1U, &config) == NETWORK_CONFIG_TOO_LARGE,
           "oversized config rejected before copy");

    config = (network_config_t) {
        .ssid         = "new \\\"wifi",
        .password     = "new\\\\secret",
        .auto_connect = true,
    };
    char updated[NETWORK_CONFIG_FILE_MAX + 1U];
    size_t updated_length = 0U;
    expect(network_config_format_update(valid, sizeof(valid) - 1U, &config, updated, sizeof(updated),
                                        &updated_length) == NETWORK_CONFIG_OK,
           "config update formats");
    expect(strstr(updated, "# TabOS network") != NULL, "comment preserved");
    expect(strstr(updated, "[dns]\nservers=\"future\"") != NULL, "future section preserved");
    network_config_t reparsed;
    expect(network_config_parse(updated, updated_length, &reparsed) == NETWORK_CONFIG_OK, "updated config reparses");
    expect(strcmp(reparsed.ssid, config.ssid) == 0, "updated SSID round trips");
    expect(strcmp(reparsed.password, config.password) == 0, "updated password round trips");
    expect(reparsed.auto_connect, "updated autoconnect round trips");
    return failures == 0 ? 0 : 1;
}
