#include <tabos/internal/network.h>

#include <tabos/internal/network_config.h>
#include <tabos/internal/time.h>
#include <tabos/platform/platform.h>

#include <stdio.h>
#include <stdatomic.h>
#include <string.h>

enum {
    NETWORK_AUTOCONNECT_ATTEMPTS = 3,
    NETWORK_RETRY_DELAY_MS       = 1000,
};

static network_status_t current;
static char password[NETWORK_CONFIG_PASSWORD_MAX + 1U];
static bool initialized;
static bool retry_suppressed;
static tabos_timer_t retry_timer;
static atomic_bool status_pending;
static platform_mutex_t* state_mutex;
static platform_mutex_t* operation_mutex;

static void network_event(void)
{
    atomic_store_explicit(&status_pending, true, memory_order_release);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_NETWORK);
}

static void set_failure(const char* failure)
{
    current.state = NETWORK_STATE_FAILED;
    if (failure == NULL || failure[0] == '\0') {
        failure = "connection failed";
    }
    (void) snprintf(current.last_failure, sizeof(current.last_failure), "%s", failure);
}

static bool prepare_attempt(char* attempt_ssid, char* attempt_password)
{
    if (current.ssid[0] == '\0' || current.attempts >= NETWORK_AUTOCONNECT_ATTEMPTS) {
        set_failure("autoconnect attempts exhausted");
        return false;
    }
    ++current.attempts;
    current.state           = NETWORK_STATE_CONNECTING;
    current.last_failure[0] = '\0';
    (void) snprintf(attempt_ssid, NETWORK_CONFIG_SSID_MAX + 1U, "%s", current.ssid);
    (void) snprintf(attempt_password, NETWORK_CONFIG_PASSWORD_MAX + 1U, "%s", password);
    return true;
}

static bool perform_attempt(void)
{
    char attempt_ssid[NETWORK_CONFIG_SSID_MAX + 1U];
    char attempt_password[NETWORK_CONFIG_PASSWORD_MAX + 1U];
    platform_mutex_lock(state_mutex);
    const bool prepared = prepare_attempt(attempt_ssid, attempt_password);
    platform_mutex_unlock(state_mutex);
    if (!prepared) {
        return false;
    }

    const bool accepted = platform_network_connect(attempt_ssid, attempt_password);
    if (!accepted) {
        platform_mutex_lock(state_mutex);
        set_failure("platform rejected connection");
        platform_mutex_unlock(state_mutex);
    }
    memset(attempt_password, 0, sizeof(attempt_password));
    return accepted;
}

bool network_service_init(void)
{
    if (initialized) {
        return true;
    }
    state_mutex = platform_mutex_create();
    if (state_mutex == NULL) {
        return false;
    }
    operation_mutex = platform_mutex_create();
    if (operation_mutex == NULL) {
        platform_mutex_destroy(state_mutex);
        state_mutex = NULL;
        return false;
    }
    current = (network_status_t) {.state = NETWORK_STATE_STARTING};
    tabos_timer_cancel(&retry_timer);
    retry_suppressed        = false;
    network_config_t config = {
        .name = NETWORK_CONFIG_DEFAULT_NAME,
    };
    const network_config_result_t result = network_config_load(&config);
    (void) snprintf(current.hostname, sizeof(current.hostname), "%s", config.name);
    if (!platform_network_init(config.name, network_event)) {
        set_failure("network backend unavailable");
        initialized = true;
        return true;
    }
    if (!platform_network_operations_init() || !platform_network_socket_operations_init() ||
        !platform_tls_operations_init()) {
        platform_tls_operations_shutdown();
        platform_network_socket_operations_shutdown();
        platform_network_operations_shutdown();
        platform_network_shutdown();
        set_failure("network operations unavailable");
        initialized = true;
        return true;
    }
    current.state = NETWORK_STATE_OFFLINE;
    if (result == NETWORK_CONFIG_OK) {
        current.config_available = true;
        current.auto_connect     = config.auto_connect;
        (void) snprintf(current.ssid, sizeof(current.ssid), "%s", config.ssid);
        (void) snprintf(password, sizeof(password), "%s", config.password);
        if (config.auto_connect) {
            platform_mutex_lock(operation_mutex);
            (void) perform_attempt();
            platform_mutex_unlock(operation_mutex);
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
    if (operation_mutex == NULL || state_mutex == NULL) {
        return;
    }

    platform_mutex_lock(operation_mutex);
    platform_mutex_lock(state_mutex);
    if (!initialized) {
        platform_mutex_unlock(state_mutex);
        platform_mutex_unlock(operation_mutex);
        return;
    }
    const bool status_requested = atomic_exchange_explicit(&status_pending, false, memory_order_acq_rel);
    platform_mutex_unlock(state_mutex);

    platform_network_status_t platform_status;
    bool status_available = false;
    if (status_requested) {
        status_available = platform_network_status(&platform_status);
    }

    platform_mutex_lock(state_mutex);
    if (status_requested) {
        if (!status_available) {
            set_failure("network status unavailable");
        } else if (platform_status.state == PLATFORM_NETWORK_ONLINE) {
            current.state = NETWORK_STATE_ONLINE;
            tabos_timer_cancel(&retry_timer);
            (void) snprintf(current.ipv4, sizeof(current.ipv4), "%s", platform_status.ipv4);
            current.signal_dbm = platform_status.signal_dbm;
        } else if (platform_status.state == PLATFORM_NETWORK_CONNECTING) {
            current.state = NETWORK_STATE_CONNECTING;
        } else if (platform_status.state == PLATFORM_NETWORK_FAILED && !retry_suppressed) {
            const bool was_connecting = current.state == NETWORK_STATE_CONNECTING;
            set_failure(platform_status.failure);
            if (was_connecting && current.attempts < NETWORK_AUTOCONNECT_ATTEMPTS) {
                tabos_timer_start(&retry_timer, NETWORK_RETRY_DELAY_MS, 0U);
                platform_runtime_notify(PLATFORM_RUNTIME_EVENT_DEADLINE);
            }
        } else if (platform_status.state == PLATFORM_NETWORK_OFFLINE && !retry_suppressed) {
            current.state   = NETWORK_STATE_OFFLINE;
            current.ipv4[0] = '\0';
        }
    }
    const bool retry_ready = tabos_timer_poll(&retry_timer);
    platform_mutex_unlock(state_mutex);
    if (retry_ready) {
        (void) perform_attempt();
    }
    platform_mutex_unlock(operation_mutex);
}

uint64_t network_service_next_deadline(void)
{
    if (state_mutex == NULL) {
        return TIME_DEADLINE_NONE;
    }
    platform_mutex_lock(state_mutex);
    const uint64_t deadline = initialized ? time_timer_deadline(&retry_timer) : TIME_DEADLINE_NONE;
    platform_mutex_unlock(state_mutex);
    return deadline;
}

void network_service_shutdown(void)
{
    if (operation_mutex == NULL || state_mutex == NULL) {
        return;
    }

    platform_mutex_t* state     = state_mutex;
    platform_mutex_t* operation = operation_mutex;
    platform_mutex_lock(operation);
    platform_mutex_lock(state);
    if (!initialized) {
        platform_mutex_unlock(state);
        platform_mutex_unlock(operation);
        return;
    }
    initialized = false;
    tabos_timer_cancel(&retry_timer);
    retry_suppressed = true;
    platform_mutex_unlock(state);

    (void) platform_network_disconnect();
    platform_tls_operations_shutdown();
    platform_network_socket_operations_shutdown();
    platform_network_operations_shutdown();
    platform_network_shutdown();

    platform_mutex_lock(state);
    memset(password, 0, sizeof(password));
    current = (network_status_t) {0};
    atomic_store_explicit(&status_pending, false, memory_order_release);
    state_mutex     = NULL;
    operation_mutex = NULL;
    platform_mutex_unlock(state);
    platform_mutex_unlock(operation);
    platform_mutex_destroy(operation);
    platform_mutex_destroy(state);
}

bool network_service_connect(const char* ssid, const char* supplied_password, bool automatic)
{
    if (ssid == NULL || supplied_password == NULL || ssid[0] == '\0' || strlen(ssid) > NETWORK_CONFIG_SSID_MAX ||
        strlen(supplied_password) > NETWORK_CONFIG_PASSWORD_MAX) {
        return false;
    }

    if (operation_mutex == NULL || state_mutex == NULL) {
        return false;
    }
    platform_mutex_lock(operation_mutex);
    platform_mutex_lock(state_mutex);
    if (!initialized) {
        platform_mutex_unlock(state_mutex);
        platform_mutex_unlock(operation_mutex);
        return false;
    }
    tabos_timer_cancel(&retry_timer);
    retry_suppressed = true;
    current.state    = NETWORK_STATE_DISCONNECTING;
    platform_mutex_unlock(state_mutex);

    (void) platform_network_disconnect();
    platform_mutex_lock(state_mutex);
    (void) snprintf(current.ssid, sizeof(current.ssid), "%s", ssid);
    (void) snprintf(password, sizeof(password), "%s", supplied_password);
    current.auto_connect = automatic;
    current.attempts     = 0U;
    current.ipv4[0]      = '\0';
    retry_suppressed     = false;
    platform_mutex_unlock(state_mutex);
    const bool started = perform_attempt();
    platform_mutex_unlock(operation_mutex);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_DEADLINE);
    return started;
}

bool network_service_disconnect(void)
{
    if (operation_mutex == NULL || state_mutex == NULL) {
        return false;
    }
    platform_mutex_lock(operation_mutex);
    platform_mutex_lock(state_mutex);
    if (!initialized) {
        platform_mutex_unlock(state_mutex);
        platform_mutex_unlock(operation_mutex);
        return false;
    }
    tabos_timer_cancel(&retry_timer);
    retry_suppressed = true;
    current.state    = NETWORK_STATE_DISCONNECTING;
    platform_mutex_unlock(state_mutex);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (!platform_network_disconnect()) {
        platform_mutex_lock(state_mutex);
        set_failure("disconnect failed");
        platform_mutex_unlock(state_mutex);
        platform_mutex_unlock(operation_mutex);
        return false;
    }
    platform_mutex_lock(state_mutex);
    current.state   = NETWORK_STATE_OFFLINE;
    current.ipv4[0] = '\0';
    platform_mutex_unlock(state_mutex);
    platform_mutex_unlock(operation_mutex);
    return true;
}

bool network_service_status(network_status_t* status)
{
    if (status == NULL || state_mutex == NULL) {
        return false;
    }
    platform_mutex_lock(state_mutex);
    if (!initialized) {
        platform_mutex_unlock(state_mutex);
        return false;
    }
    *status = current;
    platform_mutex_unlock(state_mutex);
    return true;
}

network_operation_result_t network_service_resolve(const char* hostname, uint32_t family, network_address_t* address)
{
    if (hostname == NULL || address == NULL || hostname[0] == '\0' || (family != 0U && family != 4U && family != 6U)) {
        return NETWORK_OPERATION_INVALID;
    }
    if (state_mutex == NULL) {
        return NETWORK_OPERATION_OFFLINE;
    }
    platform_mutex_lock(state_mutex);
    const bool online = initialized && current.state == NETWORK_STATE_ONLINE;
    platform_mutex_unlock(state_mutex);
    if (!online) {
        return NETWORK_OPERATION_OFFLINE;
    }
    platform_network_address_t platform_address;
    const platform_network_operation_result_t result = platform_network_resolve(hostname, family, &platform_address);
    if (result == PLATFORM_NETWORK_OPERATION_OK) {
        address->family = platform_address.family;
        (void) snprintf(address->text, sizeof(address->text), "%s", platform_address.text);
    }
    return (network_operation_result_t) result;
}

network_operation_result_t network_service_echo(const network_address_t* address, uint16_t sequence,
                                                uint16_t payload_bytes, uint32_t timeout_ms,
                                                network_echo_result_t* result)
{
    if (address == NULL || result == NULL || (address->family != 4U && address->family != 6U) ||
        address->text[0] == '\0' || memchr(address->text, '\0', sizeof(address->text)) == NULL ||
        payload_bytes > 1024U || timeout_ms == 0U || timeout_ms > 60000U) {
        return NETWORK_OPERATION_INVALID;
    }
    if (state_mutex == NULL) {
        return NETWORK_OPERATION_OFFLINE;
    }
    platform_mutex_lock(state_mutex);
    const bool online = initialized && current.state == NETWORK_STATE_ONLINE;
    platform_mutex_unlock(state_mutex);
    if (!online) {
        return NETWORK_OPERATION_OFFLINE;
    }
    const platform_network_address_t platform_address = {.family = address->family};
    platform_network_address_t copied_address         = platform_address;
    (void) snprintf(copied_address.text, sizeof(copied_address.text), "%s", address->text);
    platform_network_echo_result_t platform_result;
    const platform_network_operation_result_t operation =
        platform_network_echo(&copied_address, sequence, payload_bytes, timeout_ms, &platform_result);
    if (operation == PLATFORM_NETWORK_OPERATION_OK) {
        result->sequence      = platform_result.sequence;
        result->bytes         = platform_result.bytes;
        result->round_trip_ms = platform_result.round_trip_ms;
    }
    return (network_operation_result_t) operation;
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
