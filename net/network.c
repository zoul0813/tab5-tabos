#include <tabos/internal/network.h>

#include <tabos/internal/network_config.h>
#include <tabos/internal/time.h>
#include <tabos/platform/platform.h>

#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

enum {
    NETWORK_AUTOCONNECT_ATTEMPTS = 3,
    NETWORK_RETRY_DELAY_MS       = 1000,
    /* ESP-Hosted keeps retrying a missing or rejecting AP without reporting a
     * final disconnect, so each attempt needs its own bound. Normal Tab5
     * association reaches DHCP in about 7 seconds. */
    NETWORK_ATTEMPT_TIMEOUT_MS = 20000,
};

_Static_assert((int) NETWORK_SCAN_MAX == (int) PLATFORM_WIFI_SCAN_MAX, "portable and platform scan bounds must match");
_Static_assert((int) NETWORK_OPERATION_BUSY == (int) PLATFORM_NETWORK_OPERATION_BUSY,
               "portable and platform operation results must match");

static network_status_t current;
static char password[NETWORK_CONFIG_PASSWORD_MAX + 1U];
static bool initialized;
static bool backend_ready;
static bool retry_suppressed;
static tabos_timer_t retry_timer;
static tabos_timer_t attempt_timer;
static atomic_bool status_pending;
static platform_mutex_t* state_mutex;
static platform_mutex_t* operation_mutex;
static platform_mutex_t* profile_mutex;

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

/* Diagnostics name the attempt and network state only; never credentials. */
static void log_attempt(const char* event, unsigned int attempt, const char* detail)
{
    char message[128];
    (void) snprintf(message, sizeof(message), "Network: attempt %u/%u %s%s%s", attempt,
                    (unsigned int) NETWORK_AUTOCONNECT_ATTEMPTS, event, detail[0] != '\0' ? ": " : "", detail);
    platform_log(message);
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
    platform_mutex_lock(state_mutex);
    log_attempt(accepted ? "started" : "rejected", current.attempts, "");
    if (accepted) {
        tabos_timer_start(&attempt_timer, NETWORK_ATTEMPT_TIMEOUT_MS, 0U);
    } else {
        /* A rejected start (radio still busy) counts as a failed attempt and is
         * retried like one, so the sequence always ends online or finally failed. */
        set_failure("platform rejected connection");
        if (current.attempts < NETWORK_AUTOCONNECT_ATTEMPTS && !retry_suppressed) {
            tabos_timer_start(&retry_timer, NETWORK_RETRY_DELAY_MS, 0U);
        }
    }
    platform_mutex_unlock(state_mutex);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_DEADLINE);
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
    profile_mutex = platform_mutex_create();
    if (profile_mutex == NULL) {
        platform_mutex_destroy(operation_mutex);
        platform_mutex_destroy(state_mutex);
        operation_mutex = NULL;
        state_mutex     = NULL;
        return false;
    }
    current = (network_status_t) {.state = NETWORK_STATE_STARTING};
    tabos_timer_cancel(&retry_timer);
    tabos_timer_cancel(&attempt_timer);
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
    backend_ready = true;
    if (result == NETWORK_CONFIG_NO_PROFILE) {
        current.auto_connect = config.auto_connect;
    } else if (result == NETWORK_CONFIG_OK) {
        current.config_available = true;
        current.auto_connect     = config.auto_connect;
        (void) snprintf(current.ssid, sizeof(current.ssid), "%s", config.ssid);
        (void) snprintf(current.saved_ssid, sizeof(current.saved_ssid), "%s", config.ssid);
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

    bool stop_radio = false;
    platform_mutex_lock(state_mutex);
    if (status_requested) {
        if (!status_available) {
            set_failure("network status unavailable");
        } else if (platform_status.state == PLATFORM_NETWORK_ONLINE) {
            current.state = NETWORK_STATE_ONLINE;
            tabos_timer_cancel(&retry_timer);
            tabos_timer_cancel(&attempt_timer);
            (void) snprintf(current.ipv4, sizeof(current.ipv4), "%s", platform_status.ipv4);
            current.signal_dbm = platform_status.signal_dbm;
        } else if (platform_status.state == PLATFORM_NETWORK_CONNECTING) {
            current.state = NETWORK_STATE_CONNECTING;
        } else if (platform_status.state == PLATFORM_NETWORK_FAILED && !retry_suppressed) {
            const bool was_connecting = current.state == NETWORK_STATE_CONNECTING;
            tabos_timer_cancel(&attempt_timer);
            set_failure(platform_status.failure);
            if (was_connecting) {
                log_attempt("failed", current.attempts, current.last_failure);
            }
            if (was_connecting && current.attempts < NETWORK_AUTOCONNECT_ATTEMPTS) {
                tabos_timer_start(&retry_timer, NETWORK_RETRY_DELAY_MS, 0U);
                platform_runtime_notify(PLATFORM_RUNTIME_EVENT_DEADLINE);
            } else if (was_connecting) {
                /* Final failure: stop ESP-Hosted from retrying in the background. */
                stop_radio = true;
            }
        } else if (platform_status.state == PLATFORM_NETWORK_OFFLINE && !retry_suppressed) {
            /* A timed-out attempt is stopped with a disconnect; keep its failure. */
            if (current.state != NETWORK_STATE_FAILED) {
                current.state = NETWORK_STATE_OFFLINE;
            }
            current.ipv4[0] = '\0';
        }
    }
    const bool attempt_expired   = tabos_timer_poll(&attempt_timer);
    const bool attempt_timed_out = attempt_expired && current.state == NETWORK_STATE_CONNECTING;
    if (attempt_expired && !attempt_timed_out) {
        char detail[32];
        (void) snprintf(detail, sizeof(detail), "state %s", network_state_name(current.state));
        log_attempt("timer expired outside connecting", current.attempts, detail);
    }
    if (attempt_timed_out) {
        set_failure("connection timed out (network not found or rejected)");
        log_attempt("timed out", current.attempts, "");
        if (current.attempts < NETWORK_AUTOCONNECT_ATTEMPTS) {
            tabos_timer_start(&retry_timer, NETWORK_RETRY_DELAY_MS, 0U);
            platform_runtime_notify(PLATFORM_RUNTIME_EVENT_DEADLINE);
        }
    }
    const bool retry_ready = tabos_timer_poll(&retry_timer);
    platform_mutex_unlock(state_mutex);
    if (attempt_timed_out || stop_radio) {
        (void) platform_network_disconnect();
    }
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
    uint64_t deadline = TIME_DEADLINE_NONE;
    if (initialized) {
        const uint64_t retry   = time_timer_deadline(&retry_timer);
        const uint64_t attempt = time_timer_deadline(&attempt_timer);
        deadline               = retry < attempt ? retry : attempt;
    }
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
    platform_mutex_t* profile   = profile_mutex;
    platform_mutex_lock(profile);
    platform_mutex_lock(operation);
    platform_mutex_lock(state);
    if (!initialized) {
        platform_mutex_unlock(state);
        platform_mutex_unlock(operation);
        platform_mutex_unlock(profile);
        return;
    }
    initialized   = false;
    backend_ready = false;
    tabos_timer_cancel(&retry_timer);
    tabos_timer_cancel(&attempt_timer);
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
    profile_mutex   = NULL;
    platform_mutex_unlock(state);
    platform_mutex_unlock(operation);
    platform_mutex_unlock(profile);
    platform_mutex_destroy(operation);
    platform_mutex_destroy(state);
    platform_mutex_destroy(profile);
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
    tabos_timer_cancel(&attempt_timer);
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
    tabos_timer_cancel(&attempt_timer);
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

static bool access_point_precedes(const platform_wifi_access_point_t* left, const platform_wifi_access_point_t* right)
{
    if (left->signal_dbm != right->signal_dbm) {
        return left->signal_dbm > right->signal_dbm;
    }
    if (left->channel != right->channel) {
        return left->channel < right->channel;
    }
    return memcmp(left->bssid, right->bssid, sizeof(left->bssid)) < 0;
}

static void sort_access_points(platform_wifi_access_point_t* access_points, uint32_t count)
{
    for (uint32_t index = 1U; index < count; ++index) {
        const platform_wifi_access_point_t candidate = access_points[index];
        uint32_t position                            = index;
        while (position > 0U && access_point_precedes(&candidate, &access_points[position - 1U])) {
            access_points[position] = access_points[position - 1U];
            --position;
        }
        access_points[position] = candidate;
    }
}

network_operation_result_t network_service_scan(network_access_point_t* access_points, uint32_t capacity,
                                                uint32_t* count)
{
    if (access_points == NULL || count == NULL || capacity == 0U) {
        return NETWORK_OPERATION_INVALID;
    }
    *count = 0U;
    if (capacity > NETWORK_SCAN_MAX) {
        capacity = NETWORK_SCAN_MAX;
    }
    if (state_mutex == NULL) {
        return NETWORK_OPERATION_OFFLINE;
    }
    platform_mutex_lock(state_mutex);
    const bool ready = initialized && backend_ready;
    platform_mutex_unlock(state_mutex);
    if (!ready) {
        return NETWORK_OPERATION_OFFLINE;
    }

    /* Request the full bound so ordering never depends on backend truncation. */
    platform_wifi_access_point_t* found = calloc(PLATFORM_WIFI_SCAN_MAX, sizeof(*found));
    if (found == NULL) {
        return NETWORK_OPERATION_IO;
    }
    uint32_t found_count = 0U;
    const platform_network_operation_result_t operation =
        platform_network_scan(found, PLATFORM_WIFI_SCAN_MAX, &found_count);
    if (operation == PLATFORM_NETWORK_OPERATION_OK) {
        if (found_count > PLATFORM_WIFI_SCAN_MAX) {
            found_count = PLATFORM_WIFI_SCAN_MAX;
        }
        sort_access_points(found, found_count);
        const uint32_t copied = found_count < capacity ? found_count : capacity;
        for (uint32_t index = 0U; index < copied; ++index) {
            network_access_point_t* target = &access_points[index];
            memset(target, 0, sizeof(*target));
            memcpy(target->ssid, found[index].ssid, sizeof(target->ssid) - 1U);
            memcpy(target->bssid, found[index].bssid, sizeof(target->bssid));
            target->channel    = found[index].channel;
            target->signal_dbm = found[index].signal_dbm;
            target->security   = found[index].security;
            target->phy        = found[index].phy;
        }
        *count = copied;
    }
    free(found);
    return (network_operation_result_t) operation;
}

static network_profile_result_t profile_result(network_config_result_t result)
{
    switch (result) {
        case NETWORK_CONFIG_OK:
        case NETWORK_CONFIG_NO_PROFILE:
        case NETWORK_CONFIG_NOT_FOUND: return NETWORK_PROFILE_OK;
        case NETWORK_CONFIG_UNAVAILABLE: return NETWORK_PROFILE_UNAVAILABLE;
        case NETWORK_CONFIG_INVALID:
        case NETWORK_CONFIG_TOO_LARGE: return NETWORK_PROFILE_INVALID;
        case NETWORK_CONFIG_IO_ERROR: return NETWORK_PROFILE_IO;
    }
    return NETWORK_PROFILE_IO;
}

/* Loads the saved file for modification. A missing file starts from defaults
 * using the current hostname; damaged files are never overwritten. */
static network_profile_result_t load_profile(network_config_t* config, const char* hostname)
{
    *config = (network_config_t) {.auto_connect = true};
    (void) snprintf(config->name, sizeof(config->name), "%s", hostname);
    const network_config_result_t loaded = network_config_load(config);
    if (loaded == NETWORK_CONFIG_NOT_FOUND) {
        *config = (network_config_t) {.auto_connect = true};
        (void) snprintf(config->name, sizeof(config->name), "%s", hostname);
    }
    return profile_result(loaded);
}

static bool profile_begin(char* hostname, size_t capacity)
{
    if (state_mutex == NULL || profile_mutex == NULL) {
        return false;
    }
    platform_mutex_lock(profile_mutex);
    platform_mutex_lock(state_mutex);
    const bool ready = initialized;
    (void) snprintf(hostname, capacity, "%s", current.hostname);
    platform_mutex_unlock(state_mutex);
    if (!ready) {
        platform_mutex_unlock(profile_mutex);
    }
    return ready;
}

network_profile_result_t network_service_save_current(void)
{
    char hostname[sizeof(current.hostname)];
    if (!profile_begin(hostname, sizeof(hostname))) {
        return NETWORK_PROFILE_NOT_CONNECTED;
    }
    char ssid[NETWORK_CONFIG_SSID_MAX + 1U];
    char secret[NETWORK_CONFIG_PASSWORD_MAX + 1U];
    platform_mutex_lock(state_mutex);
    const bool online = current.state == NETWORK_STATE_ONLINE && current.ssid[0] != '\0';
    (void) snprintf(ssid, sizeof(ssid), "%s", current.ssid);
    (void) snprintf(secret, sizeof(secret), "%s", password);
    platform_mutex_unlock(state_mutex);
    if (!online) {
        memset(secret, 0, sizeof(secret));
        platform_mutex_unlock(profile_mutex);
        return NETWORK_PROFILE_NOT_CONNECTED;
    }

    network_config_t config;
    network_profile_result_t result = load_profile(&config, hostname);
    if (result == NETWORK_PROFILE_OK) {
        (void) snprintf(config.ssid, sizeof(config.ssid), "%s", ssid);
        (void) snprintf(config.password, sizeof(config.password), "%s", secret);
        result = profile_result(network_config_save(&config));
    }
    if (result == NETWORK_PROFILE_OK) {
        platform_mutex_lock(state_mutex);
        current.config_available = true;
        current.auto_connect     = config.auto_connect;
        (void) snprintf(current.saved_ssid, sizeof(current.saved_ssid), "%s", ssid);
        platform_mutex_unlock(state_mutex);
    }
    memset(secret, 0, sizeof(secret));
    memset(&config, 0, sizeof(config));
    platform_mutex_unlock(profile_mutex);
    return result;
}

network_profile_result_t network_service_forget(void)
{
    char hostname[sizeof(current.hostname)];
    if (!profile_begin(hostname, sizeof(hostname))) {
        return NETWORK_PROFILE_NOT_CONNECTED;
    }
    network_config_t config;
    const network_config_result_t loaded = network_config_load(&config);
    network_profile_result_t result      = profile_result(loaded);
    if (loaded == NETWORK_CONFIG_OK) {
        memset(config.ssid, 0, sizeof(config.ssid));
        memset(config.password, 0, sizeof(config.password));
        result = profile_result(network_config_save(&config));
    }
    if (result == NETWORK_PROFILE_OK) {
        platform_mutex_lock(state_mutex);
        current.config_available = false;
        current.saved_ssid[0]    = '\0';
        platform_mutex_unlock(state_mutex);
    }
    memset(&config, 0, sizeof(config));
    platform_mutex_unlock(profile_mutex);
    return result;
}

network_profile_result_t network_service_set_auto_connect(bool enabled)
{
    char hostname[sizeof(current.hostname)];
    if (!profile_begin(hostname, sizeof(hostname))) {
        return NETWORK_PROFILE_NOT_CONNECTED;
    }
    network_config_t config;
    network_profile_result_t result = load_profile(&config, hostname);
    if (result == NETWORK_PROFILE_OK) {
        config.auto_connect = enabled;
        result              = profile_result(network_config_save(&config));
    }
    if (result == NETWORK_PROFILE_OK) {
        platform_mutex_lock(state_mutex);
        current.auto_connect = enabled;
        platform_mutex_unlock(state_mutex);
    }
    memset(&config, 0, sizeof(config));
    platform_mutex_unlock(profile_mutex);
    return result;
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
