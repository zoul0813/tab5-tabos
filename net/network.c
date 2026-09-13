#include <tabos/internal/network.h>

#include <tabos/internal/network_config.h>
#include <tabos/internal/time.h>
#include <tabos/internal/service_admission.h>
#include <tabos/filesystem.h>
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
static bool backend_ready;
static bool connection_requested;
static bool retry_suppressed;
static tabos_timer_t retry_timer;
static atomic_bool status_pending;
static service_admission_t power_admission = {.state = SERVICE_ADMISSION_FROZEN};
static bool power_suspended;
static bool reconnect_after_resume;
static platform_mutex_t* state_mutex;
static platform_signal_t* drained_signal;
/* Serializes control transactions without holding the state mutex in a driver.
 * Readers remain available; competing control requests fail without side effects. */
static bool control_busy;
static bool update_deferred;

static bool begin_call(void)
{
    if (!service_admission_enter(&power_admission, false)) {
        return false;
    }
    platform_mutex_lock(state_mutex);
    return true;
}

static void finish_call(void)
{
    service_admission_leave(&power_admission, false);
    /* Notify before dropping the mutex: shutdown must not destroy the signal
     * between the final admission decrement and this notification. */
    if ((atomic_load(&power_admission.state) & SERVICE_ADMISSION_FROZEN) != 0U) {
        platform_signal_notify(drained_signal);
    }
    platform_mutex_unlock(state_mutex);
}

static void finish_control(void)
{
    control_busy = false;
    if (update_deferred) {
        update_deferred = false;
        platform_runtime_notify(PLATFORM_RUNTIME_EVENT_NETWORK | PLATFORM_RUNTIME_EVENT_DEADLINE);
    }
}

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

static bool start_attempt(void)
{
    if (current.ssid[0] == '\0' || current.attempts >= NETWORK_AUTOCONNECT_ATTEMPTS) {
        set_failure("autoconnect attempts exhausted");
        return false;
    }
    ++current.attempts;
    current.state           = NETWORK_STATE_CONNECTING;
    current.last_failure[0] = '\0';
    char attempt_ssid[sizeof(current.ssid)];
    char attempt_password[sizeof(password)];
    memcpy(attempt_ssid, current.ssid, sizeof(attempt_ssid));
    memcpy(attempt_password, password, sizeof(attempt_password));
    platform_mutex_unlock(state_mutex);
    const bool connected = platform_network_connect(attempt_ssid, attempt_password);
    memset(attempt_password, 0, sizeof(attempt_password));
    platform_mutex_lock(state_mutex);
    if (!connected) {
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
    state_mutex    = platform_mutex_create();
    drained_signal = platform_signal_create();
    if (state_mutex == NULL || drained_signal == NULL) {
        platform_signal_destroy(drained_signal);
        platform_mutex_destroy(state_mutex);
        state_mutex    = NULL;
        drained_signal = NULL;
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
        service_admission_freeze(&power_admission, false);
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
        service_admission_freeze(&power_admission, false);
        return true;
    }
    current.state = NETWORK_STATE_OFFLINE;
    backend_ready = true;
    if (result == NETWORK_CONFIG_OK) {
        current.config_available = true;
        current.auto_connect     = config.auto_connect;
        (void) snprintf(current.ssid, sizeof(current.ssid), "%s", config.ssid);
        (void) snprintf(password, sizeof(password), "%s", config.password);
        if (config.auto_connect) {
            connection_requested = true;
            platform_mutex_lock(state_mutex);
            (void) start_attempt();
            platform_mutex_unlock(state_mutex);
        }
    } else if (result != NETWORK_CONFIG_NOT_FOUND && result != NETWORK_CONFIG_UNAVAILABLE) {
        (void) snprintf(current.last_failure, sizeof(current.last_failure), "wifi.conf: %s",
                        network_config_result_name(result));
    }
    initialized = true;
    service_admission_freeze(&power_admission, false);
    return true;
}

static void update_active(void)
{
    if (!initialized) {
        return;
    }
    if (atomic_exchange_explicit(&status_pending, false, memory_order_acq_rel)) {
        platform_network_status_t platform_status;
        platform_mutex_unlock(state_mutex);
        const bool available = platform_network_status(&platform_status);
        platform_mutex_lock(state_mutex);
        if (!available) {
            set_failure("network status unavailable");
        } else if (platform_status.state == PLATFORM_NETWORK_ONLINE && !retry_suppressed) {
            current.state = NETWORK_STATE_ONLINE;
            tabos_timer_cancel(&retry_timer);
            (void) snprintf(current.ipv4, sizeof(current.ipv4), "%s", platform_status.ipv4);
            current.signal_dbm = platform_status.signal_dbm;
        } else if (platform_status.state == PLATFORM_NETWORK_CONNECTING && !retry_suppressed) {
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
    if (tabos_timer_poll(&retry_timer)) {
        (void) start_attempt();
    }
}

uint64_t network_service_next_deadline(void)
{
    if (!begin_call()) {
        return TIME_DEADLINE_NONE;
    }
    const uint64_t deadline = !control_busy ? time_timer_deadline(&retry_timer) : TIME_DEADLINE_NONE;
    if (control_busy) {
        update_deferred = true;
    }
    finish_call();
    return deadline;
}

static int resume_active(bool reopen);

void network_service_shutdown(void)
{
    if (!initialized) {
        return;
    }
    service_admission_freeze(&power_admission, true);
    platform_mutex_lock(state_mutex);
    while ((atomic_load(&power_admission.state) & SERVICE_ADMISSION_MASK) != 0U) {
        platform_mutex_unlock(state_mutex);
        platform_signal_wait(drained_signal, UINT32_MAX);
        platform_mutex_lock(state_mutex);
    }
    platform_mutex_unlock(state_mutex);
    (void) resume_active(false);
    tabos_timer_cancel(&retry_timer);
    (void) platform_network_disconnect();
    platform_tls_operations_shutdown();
    platform_network_socket_operations_shutdown();
    platform_network_operations_shutdown();
    platform_network_shutdown();
    memset(password, 0, sizeof(password));
    current = (network_status_t) {0};
    atomic_store_explicit(&status_pending, false, memory_order_release);
    initialized            = false;
    backend_ready          = false;
    connection_requested   = false;
    power_suspended        = false;
    reconnect_after_resume = false;
    control_busy           = false;
    update_deferred        = false;
    platform_signal_destroy(drained_signal);
    platform_mutex_destroy(state_mutex);
    drained_signal = NULL;
    state_mutex    = NULL;
}

static bool connect_active(const char* ssid, const char* supplied_password, bool automatic)
{
    if (!initialized || ssid == NULL || supplied_password == NULL || ssid[0] == '\0' ||
        strlen(ssid) > NETWORK_CONFIG_SSID_MAX || strlen(supplied_password) > NETWORK_CONFIG_PASSWORD_MAX) {
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
    connection_requested = true;
    current.attempts     = 0U;
    current.ipv4[0]      = '\0';
    retry_suppressed   = false;
    const bool started = start_attempt();
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_DEADLINE);
    return started;
}

static bool disconnect_active(void)
{
    if (!initialized) {
        return false;
    }
    tabos_timer_cancel(&retry_timer);
    retry_suppressed     = true;
    connection_requested = false;
    current.state        = NETWORK_STATE_DISCONNECTING;
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_DEADLINE);
    platform_mutex_unlock(state_mutex);
    const bool disconnected = platform_network_disconnect();
    platform_mutex_lock(state_mutex);
    if (!disconnected) {
        set_failure("disconnect failed");
        return false;
    }
    current.state   = NETWORK_STATE_OFFLINE;
    current.ipv4[0] = '\0';
    return true;
}

static bool status_active(network_status_t* status)
{
    if (!initialized || status == NULL) {
        return false;
    }
    *status = current;
    return true;
}

static network_operation_result_t resolve_active(const char* hostname, uint32_t family, network_address_t* address)
{
    if (!initialized || hostname == NULL || address == NULL || hostname[0] == '\0' ||
        (family != 0U && family != 4U && family != 6U)) {
        return NETWORK_OPERATION_INVALID;
    }
    if (current.state != NETWORK_STATE_ONLINE) {
        return NETWORK_OPERATION_OFFLINE;
    }
    platform_network_address_t platform_address;
    platform_mutex_unlock(state_mutex);
    const platform_network_operation_result_t result = platform_network_resolve(hostname, family, &platform_address);
    platform_mutex_lock(state_mutex);
    if (result == PLATFORM_NETWORK_OPERATION_OK) {
        address->family = platform_address.family;
        (void) snprintf(address->text, sizeof(address->text), "%s", platform_address.text);
    }
    return (network_operation_result_t) result;
}

static network_operation_result_t echo_active(const network_address_t* address, uint16_t sequence,
                                              uint16_t payload_bytes, uint32_t timeout_ms,
                                              network_echo_result_t* result)
{
    if (!initialized || address == NULL || result == NULL || (address->family != 4U && address->family != 6U) ||
        address->text[0] == '\0' || memchr(address->text, '\0', sizeof(address->text)) == NULL ||
        payload_bytes > 1024U || timeout_ms == 0U || timeout_ms > 60000U) {
        return NETWORK_OPERATION_INVALID;
    }
    if (current.state != NETWORK_STATE_ONLINE) {
        return NETWORK_OPERATION_OFFLINE;
    }
    const platform_network_address_t platform_address = {.family = address->family};
    platform_network_address_t copied_address         = platform_address;
    (void) snprintf(copied_address.text, sizeof(copied_address.text), "%s", address->text);
    platform_network_echo_result_t platform_result;
    platform_mutex_unlock(state_mutex);
    const platform_network_operation_result_t operation =
        platform_network_echo(&copied_address, sequence, payload_bytes, timeout_ms, &platform_result);
    platform_mutex_lock(state_mutex);
    if (operation == PLATFORM_NETWORK_OPERATION_OK) {
        result->sequence      = platform_result.sequence;
        result->bytes         = platform_result.bytes;
        result->round_trip_ms = platform_result.round_trip_ms;
    }
    return (network_operation_result_t) operation;
}

int network_service_power_suspend(void)
{
    if (!initialized || power_suspended) {
        return 0;
    }
    if (!backend_ready) {
        return -TABOS_ENOTSUP;
    }
    service_admission_freeze(&power_admission, true);
    if ((atomic_load(&power_admission.state) & SERVICE_ADMISSION_MASK) != 0U ||
        current.state == NETWORK_STATE_STARTING || current.state == NETWORK_STATE_SCANNING ||
        current.state == NETWORK_STATE_CONNECTING || current.state == NETWORK_STATE_DISCONNECTING) {
        service_admission_freeze(&power_admission, false);
        return -TABOS_EBUSY;
    }
    const int result = platform_network_power_suspend();
    if (result != 0) {
        service_admission_freeze(&power_admission, false);
        return result;
    }
    reconnect_after_resume = connection_requested && !retry_suppressed;
    power_suspended        = true;
    tabos_timer_cancel(&retry_timer);
    current.state      = NETWORK_STATE_OFFLINE;
    current.ipv4[0]    = '\0';
    current.signal_dbm = 0;
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_NETWORK | PLATFORM_RUNTIME_EVENT_DEADLINE);
    return 0;
}

static int resume_active(bool reopen)
{
    if (!power_suspended) {
        return 0;
    }
    const int result = platform_network_power_resume();
    if (result != 0) {
        set_failure("network transport restore failed");
        return result;
    }
    power_suspended         = false;
    current.state           = NETWORK_STATE_OFFLINE;
    current.last_failure[0] = '\0';
    /* Transport restored first. Connection happens later in normal dispatcher,
     * with existing three-attempt policy; AP absence is not resume failure. */
    if (reconnect_after_resume) {
        current.attempts = 0U;
        retry_suppressed = false;
        tabos_timer_start(&retry_timer, NETWORK_RETRY_DELAY_MS, 0U);
    }
    reconnect_after_resume = false;
    if (reopen) {
        service_admission_freeze(&power_admission, false);
    }
    network_event();
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_DEADLINE);
    return 0;
}

int network_service_power_resume(void)
{
    return resume_active(true);
}

void network_service_update(void)
{
    if (!begin_call()) {
        return;
    }
    if (control_busy) {
        update_deferred = true;
    } else {
        control_busy = true;
        update_active();
        finish_control();
    }
    finish_call();
}

bool network_service_connect(const char* ssid, const char* supplied_password, bool automatic)
{
    if (!begin_call()) {
        return false;
    }
    if (control_busy) {
        finish_call();
        return false;
    }
    control_busy      = true;
    const bool result = connect_active(ssid, supplied_password, automatic);
    finish_control();
    finish_call();
    return result;
}

bool network_service_disconnect(void)
{
    if (!begin_call()) {
        return false;
    }
    if (control_busy) {
        finish_call();
        return false;
    }
    control_busy      = true;
    const bool result = disconnect_active();
    finish_control();
    finish_call();
    return result;
}

bool network_service_status(network_status_t* status)
{
    if (!begin_call()) {
        return false;
    }
    const bool result = status_active(status);
    finish_call();
    return result;
}

network_operation_result_t network_service_resolve(const char* hostname, uint32_t family, network_address_t* address)
{
    if (!begin_call()) {
        return NETWORK_OPERATION_OFFLINE;
    }
    const network_operation_result_t result = resolve_active(hostname, family, address);
    finish_call();
    return result;
}

network_operation_result_t network_service_echo(const network_address_t* address, uint16_t sequence,
                                                uint16_t payload_bytes, uint32_t timeout_ms,
                                                network_echo_result_t* result)
{
    if (!begin_call()) {
        return NETWORK_OPERATION_OFFLINE;
    }
    const network_operation_result_t operation = echo_active(address, sequence, payload_bytes, timeout_ms, result);
    finish_call();
    return operation;
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
