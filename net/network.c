#include <tabos/internal/network.h>

#include <tabos/internal/network_config.h>
#include <tabos/platform/platform.h>

#include <stdio.h>
#include <string.h>

enum {
    NETWORK_AUTOCONNECT_ATTEMPTS = 3,
    NETWORK_RETRY_DELAY_MS       = 1000,
};

static network_status_t current;
static char password[NETWORK_CONFIG_PASSWORD_MAX + 1U];
static bool initialized;
static bool retry_pending;
static bool retry_suppressed;
static uint64_t retry_at_ms;

static void set_failure(const char* failure)
{
    current.state = NETWORK_STATE_FAILED;
    if (failure == NULL || failure[0] == '\0') {
        failure = "connection failed";
    }
    (void) snprintf(current.last_failure, sizeof(current.last_failure), "%s", failure);
}

static bool start_attempt(void)
{
    if (current.ssid[0] == '\0' || current.attempts >= NETWORK_AUTOCONNECT_ATTEMPTS) {
        set_failure("autoconnect attempts exhausted");
        return false;
    }
    ++current.attempts;
    current.state           = NETWORK_STATE_CONNECTING;
    current.last_failure[0] = '\0';
    if (!platform_network_connect(current.ssid, password)) {
        set_failure("platform rejected connection");
        return false;
    }
    return true;
}

bool network_service_init(void)
{
    if (initialized) {
        return true;
    }
    current          = (network_status_t) {.state = NETWORK_STATE_STARTING};
    retry_pending    = false;
    retry_suppressed = false;
    if (!platform_network_init()) {
        set_failure("network backend unavailable");
        initialized = true;
        return true;
    }
    current.state = NETWORK_STATE_OFFLINE;
    network_config_t config;
    const network_config_result_t result = network_config_load(&config);
    if (result == NETWORK_CONFIG_OK) {
        current.config_available = true;
        current.auto_connect     = config.auto_connect;
        (void) snprintf(current.ssid, sizeof(current.ssid), "%s", config.ssid);
        (void) snprintf(password, sizeof(password), "%s", config.password);
        if (config.auto_connect) {
            (void) start_attempt();
        }
    } else if (result != NETWORK_CONFIG_NOT_FOUND && result != NETWORK_CONFIG_UNAVAILABLE) {
        (void) snprintf(current.last_failure, sizeof(current.last_failure), "wifi.conf: %s",
                        network_config_result_name(result));
    }
    initialized = true;
    return true;
}

void network_service_update(void)
{
    if (!initialized) {
        return;
    }
    platform_network_status_t platform_status;
    if (!platform_network_status(&platform_status)) {
        set_failure("network status unavailable");
        return;
    }
    if (platform_status.state == PLATFORM_NETWORK_ONLINE) {
        current.state = NETWORK_STATE_ONLINE;
        retry_pending = false;
        (void) snprintf(current.ipv4, sizeof(current.ipv4), "%s", platform_status.ipv4);
        current.signal_dbm = platform_status.signal_dbm;
        return;
    }
    if (platform_status.state == PLATFORM_NETWORK_CONNECTING) {
        current.state = NETWORK_STATE_CONNECTING;
        return;
    }
    if (platform_status.state == PLATFORM_NETWORK_FAILED && !retry_suppressed) {
        const bool was_connecting = current.state == NETWORK_STATE_CONNECTING;
        set_failure(platform_status.failure);
        if (was_connecting && current.attempts < NETWORK_AUTOCONNECT_ATTEMPTS) {
            retry_pending = true;
            retry_at_ms   = platform_time_ms() + NETWORK_RETRY_DELAY_MS;
        }
    }
    if (retry_pending && platform_time_ms() >= retry_at_ms) {
        retry_pending = false;
        (void) start_attempt();
    }
}

void network_service_shutdown(void)
{
    if (!initialized) {
        return;
    }
    retry_pending = false;
    (void) platform_network_disconnect();
    platform_network_shutdown();
    memset(password, 0, sizeof(password));
    current     = (network_status_t) {0};
    initialized = false;
}

bool network_service_connect(const char* ssid, const char* supplied_password, bool automatic)
{
    if (!initialized || ssid == NULL || supplied_password == NULL || ssid[0] == '\0' ||
        strlen(ssid) > NETWORK_CONFIG_SSID_MAX || strlen(supplied_password) > NETWORK_CONFIG_PASSWORD_MAX) {
        return false;
    }
    (void) platform_network_disconnect();
    (void) snprintf(current.ssid, sizeof(current.ssid), "%s", ssid);
    (void) snprintf(password, sizeof(password), "%s", supplied_password);
    current.auto_connect = automatic;
    current.attempts     = 0U;
    current.ipv4[0]      = '\0';
    retry_pending        = false;
    retry_suppressed     = false;
    return start_attempt();
}

bool network_service_disconnect(void)
{
    if (!initialized) {
        return false;
    }
    retry_pending    = false;
    retry_suppressed = true;
    current.state    = NETWORK_STATE_DISCONNECTING;
    if (!platform_network_disconnect()) {
        set_failure("disconnect failed");
        return false;
    }
    current.state   = NETWORK_STATE_OFFLINE;
    current.ipv4[0] = '\0';
    return true;
}

bool network_service_status(network_status_t* status)
{
    if (!initialized || status == NULL) {
        return false;
    }
    *status = current;
    return true;
}

const char* network_state_name(network_state_t state)
{
    switch (state) {
        case NETWORK_STATE_OFFLINE: return "offline";
        case NETWORK_STATE_STARTING: return "starting";
        case NETWORK_STATE_SCANNING: return "scanning";
        case NETWORK_STATE_CONNECTING: return "connecting";
        case NETWORK_STATE_ONLINE: return "online";
        case NETWORK_STATE_DISCONNECTING: return "disconnecting";
        case NETWORK_STATE_FAILED: return "failed";
    }
    return "unknown";
}
