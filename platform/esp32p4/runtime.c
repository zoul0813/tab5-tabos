#include "internal.h"
#include "wifi_driver.h"
#include "wifi_startup.h"

#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>

#include <esp_flash.h>
#include <esp_clk_tree.h>
#include <esp_hosted.h>
#include <esp_heap_caps.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <bsp/m5stack_tab5.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <sdkconfig.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

enum {
    WIFI_SCAN_TIMEOUT_MS = 15000,
    WIFI_SCAN_POLL_MS    = 100,
};

static const char* const TAG = TABOS_PLATFORM_LOG_TAG;
static atomic_bool stop_requested;
static bool hosted_initialized;
static atomic_bool wifi_initialized;
static tab5_wifi_driver_t wifi_driver;
static tab5_wifi_startup_t wifi_startup;
static atomic_int wifi_state;
static atomic_bool wifi_disconnect_requested;
static SemaphoreHandle_t wifi_status_mutex;
static _Atomic(TaskHandle_t) runtime_task;
static atomic_uint runtime_events;
static bool wifi_connect_pending;
static char wifi_ssid[33];
static char wifi_password[65];
static char wifi_hostname[33];
static char wifi_ipv4[16];
static char wifi_failure[64];
static platform_network_event_fn wifi_event_callback;
/* Scan primitives use static storage and are never deleted, so a scan running in
 * an application task never races semaphore destruction during shutdown. */
static StaticSemaphore_t wifi_scan_lock_storage;
static StaticSemaphore_t wifi_scan_done_storage;
static SemaphoreHandle_t wifi_scan_lock;
static SemaphoreHandle_t wifi_scan_done;
static atomic_bool wifi_scan_active;
static atomic_bool wifi_scan_failed;
static atomic_bool wifi_scan_cancel;

static void set_wifi_state(platform_network_state_t state)
{
    atomic_store_explicit(&wifi_state, state, memory_order_release);
    if (wifi_event_callback != NULL) {
        wifi_event_callback();
    }
}

static const char* disconnect_reason_text(unsigned int reason)
{
    switch (reason) {
        case WIFI_REASON_NO_AP_FOUND: return "network not found";
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_AUTH_EXPIRE: return "authentication failed";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT: return "wrong password or security mismatch";
        case WIFI_REASON_ASSOC_FAIL: return "association failed";
        case WIFI_REASON_BEACON_TIMEOUT: return "access point lost";
        default: return "Wi-Fi disconnected";
    }
}

static void network_event(void* argument, esp_event_base_t base, int32_t id, void* data)
{
    (void) argument;
    if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        const wifi_event_sta_scan_done_t* event = data;
        if (atomic_load_explicit(&wifi_scan_active, memory_order_acquire)) {
            atomic_store_explicit(&wifi_scan_failed, event != NULL && event->status != 0U, memory_order_release);
            (void) xSemaphoreGive(wifi_scan_done);
        }
        return;
    }
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t* event = data;
        if (atomic_exchange_explicit(&wifi_disconnect_requested, false, memory_order_acq_rel)) {
            /* The event of a requested disconnect can arrive after the next
             * connection started; it must not knock that attempt offline. */
            if (atomic_load_explicit(&wifi_state, memory_order_acquire) != PLATFORM_NETWORK_CONNECTING) {
                set_wifi_state(PLATFORM_NETWORK_OFFLINE);
            }
        } else {
            if (xSemaphoreTake(wifi_status_mutex, portMAX_DELAY) == pdTRUE) {
                const unsigned int reason = event != NULL ? event->reason : 0U;
                (void) snprintf(wifi_failure, sizeof(wifi_failure), "%s (reason %u)",
                                disconnect_reason_text(reason), reason);
                xSemaphoreGive(wifi_status_mutex);
            }
            set_wifi_state(PLATFORM_NETWORK_FAILED);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t* event = data;
        if (event != NULL && xSemaphoreTake(wifi_status_mutex, portMAX_DELAY) == pdTRUE) {
            (void) snprintf(wifi_ipv4, sizeof(wifi_ipv4), IPSTR, IP2STR(&event->ip_info.ip));
            xSemaphoreGive(wifi_status_mutex);
        }
        set_wifi_state(PLATFORM_NETWORK_ONLINE);
    }
}

static void tab5_network_init(void)
{
    if (bsp_feature_enable(BSP_FEATURE_WIFI, true) != ESP_OK) {
        ESP_LOGW(TAG, "Could not enable ESP32-C6 power");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
    if (esp_hosted_init() != ESP_OK) {
        ESP_LOGW(TAG, "ESP32-C6 hosted transport unavailable");
        (void) bsp_feature_enable(BSP_FEATURE_WIFI, false);
        return;
    }
    hosted_initialized = true;
}

bool platform_init(bool headless)
{
    (void) headless;
    atomic_store_explicit(&stop_requested, false, memory_order_release);
    atomic_store_explicit(&runtime_events, 0U, memory_order_release);
    atomic_store_explicit(&runtime_task, NULL, memory_order_release);
    wifi_event_callback = NULL;
    hosted_initialized  = false;
    if (!platform_usb_port_disable_host_power()) {
        ESP_LOGW(TAG, "Could not place USB-A port in safe unpowered state; continuing normal boot");
    }
    if (!platform_battery_charging_enable()) {
        ESP_LOGW(TAG, "Battery charging may remain disabled");
    }
    if (!platform_battery_set_fast_charging(false)) {
        ESP_LOGW(TAG, "Could not initialize fast-charge control");
    }
    if (!platform_battery_monitor_init()) {
        ESP_LOGW(TAG, "Battery monitor initialization failed");
    }
    (void) tab5_keyboard_init();
    (void) tab5_rtc_init();
    tab5_network_init();
    return true;
}

static bool wifi_driver_init(void)
{
    if (!tab5_wifi_driver_init(&wifi_driver, wifi_hostname, network_event)) {
        return false;
    }
    set_wifi_state(PLATFORM_NETWORK_OFFLINE);
    atomic_store_explicit(&wifi_disconnect_requested, false, memory_order_release);
    atomic_store_explicit(&wifi_initialized, true, memory_order_release);
    return true;
}

static void wifi_start_finish(void)
{
    tab5_wifi_startup_finish(&wifi_startup);
    vTaskDelete(NULL);
}

static void wifi_start(void* argument)
{
    (void) argument;
    if (esp_hosted_slave_reset() != ESP_OK) {
        ESP_LOGW(TAG, "ESP32-C6 hosted transport did not become ready");
        set_wifi_state(PLATFORM_NETWORK_FAILED);
        wifi_start_finish();
        return;
    }
    if (tab5_wifi_startup_cancelled(&wifi_startup)) {
        wifi_start_finish();
        return;
    }
    esp_hosted_coprocessor_fwver_t version;
    if (esp_hosted_get_coprocessor_fwversion(&version) == ESP_OK) {
        ESP_LOGI(TAG, "ESP32-C6 hosted firmware %" PRIu32 ".%" PRIu32 ".%" PRIu32, version.major1, version.minor1,
                 version.patch1);
    } else {
        ESP_LOGW(TAG, "ESP32-C6 hosted firmware version unavailable");
    }
    if (!wifi_driver_init()) {
        ESP_LOGW(TAG, "ESP32-C6 Wi-Fi initialization failed");
        set_wifi_state(PLATFORM_NETWORK_FAILED);
        wifi_start_finish();
        return;
    }

    if (tab5_wifi_startup_cancelled(&wifi_startup)) {
        tab5_wifi_driver_shutdown(&wifi_driver);
        atomic_store_explicit(&wifi_initialized, false, memory_order_release);
        wifi_start_finish();
        return;
    }

    wifi_config_t config = {0};
    bool connect_pending = false;
    if (xSemaphoreTake(wifi_status_mutex, portMAX_DELAY) == pdTRUE) {
        connect_pending = wifi_connect_pending;
        if (connect_pending) {
            memcpy(config.sta.ssid, wifi_ssid, strlen(wifi_ssid));
            memcpy(config.sta.password, wifi_password, strlen(wifi_password));
            if (wifi_password[0] == '\0') {
                config.sta.threshold.authmode = WIFI_AUTH_OPEN;
            }
        }
        xSemaphoreGive(wifi_status_mutex);
    }
    if (connect_pending) {
        set_wifi_state(PLATFORM_NETWORK_CONNECTING);
        if (esp_wifi_set_config(WIFI_IF_STA, &config) != ESP_OK || esp_wifi_connect() != ESP_OK) {
            set_wifi_state(PLATFORM_NETWORK_FAILED);
        }
    } else {
        set_wifi_state(PLATFORM_NETWORK_OFFLINE);
    }
    wifi_start_finish();
}

bool platform_network_init(const char* hostname, platform_network_event_fn event)
{
    if (!hosted_initialized || hostname == NULL || hostname[0] == '\0' || strlen(hostname) > 32U) {
        return false;
    }
    if (wifi_scan_lock == NULL) {
        wifi_scan_lock = xSemaphoreCreateMutexStatic(&wifi_scan_lock_storage);
    }
    if (wifi_scan_done == NULL) {
        wifi_scan_done = xSemaphoreCreateBinaryStatic(&wifi_scan_done_storage);
    }
    if (wifi_scan_lock == NULL || wifi_scan_done == NULL) {
        return false;
    }
    atomic_store_explicit(&wifi_scan_cancel, false, memory_order_release);
    wifi_status_mutex = xSemaphoreCreateMutex();
    if (wifi_status_mutex == NULL) {
        return false;
    }
    if (!tab5_wifi_startup_prepare(&wifi_startup)) {
        vSemaphoreDelete(wifi_status_mutex);
        wifi_status_mutex = NULL;
        return false;
    }
    wifi_connect_pending = false;
    wifi_event_callback  = event;
    (void) snprintf(wifi_hostname, sizeof(wifi_hostname), "%s", hostname);
    set_wifi_state(PLATFORM_NETWORK_STARTING);
    if (xTaskCreate(wifi_start, "tabos_wifi_start", 4096U, NULL, 5U, NULL) != pdPASS) {
        tab5_wifi_startup_finish(&wifi_startup);
        tab5_wifi_startup_stop_and_join(&wifi_startup);
        vSemaphoreDelete(wifi_status_mutex);
        wifi_status_mutex   = NULL;
        wifi_event_callback = NULL;
        return false;
    }
    return true;
}

void platform_network_shutdown(void)
{
    /* Stop any application scan and wait for it to leave the driver. */
    atomic_store_explicit(&wifi_scan_cancel, true, memory_order_release);
    const bool scan_locked = wifi_scan_lock != NULL && xSemaphoreTake(wifi_scan_lock, portMAX_DELAY) == pdTRUE;
    tab5_wifi_startup_stop_and_join(&wifi_startup);
    tab5_wifi_driver_shutdown(&wifi_driver);
    atomic_store_explicit(&wifi_initialized, false, memory_order_release);
    if (wifi_status_mutex != NULL) {
        vSemaphoreDelete(wifi_status_mutex);
        wifi_status_mutex = NULL;
    }
    wifi_event_callback = NULL;
    if (scan_locked) {
        (void) xSemaphoreGive(wifi_scan_lock);
    }
}

bool platform_network_connect(const char* ssid, const char* password)
{
    if (!atomic_load_explicit(&wifi_initialized, memory_order_acquire) || ssid == NULL || password == NULL ||
        strlen(ssid) > 32U || strlen(password) > 64U) {
        if (!tab5_wifi_startup_running(&wifi_startup) || ssid == NULL || password == NULL || strlen(ssid) > 32U ||
            strlen(password) > 64U) {
            return false;
        }
        if (xSemaphoreTake(wifi_status_mutex, portMAX_DELAY) == pdTRUE) {
            (void) snprintf(wifi_ssid, sizeof(wifi_ssid), "%s", ssid);
            (void) snprintf(wifi_password, sizeof(wifi_password), "%s", password);
            wifi_connect_pending = true;
            xSemaphoreGive(wifi_status_mutex);
        }
        set_wifi_state(PLATFORM_NETWORK_CONNECTING);
        return true;
    }
    wifi_config_t config = {0};
    memcpy(config.sta.ssid, ssid, strlen(ssid));
    memcpy(config.sta.password, password, strlen(password));
    if (password[0] == '\0') {
        config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }
    if (xSemaphoreTake(wifi_status_mutex, portMAX_DELAY) == pdTRUE) {
        (void) snprintf(wifi_ssid, sizeof(wifi_ssid), "%s", ssid);
        wifi_ipv4[0]    = '\0';
        wifi_failure[0] = '\0';
        xSemaphoreGive(wifi_status_mutex);
    }
    /* Keep a pending requested-disconnect flag: its event, e.g. reason 8 from
     * leaving the previous AP, is still in flight and is not this attempt failing. */
    set_wifi_state(PLATFORM_NETWORK_CONNECTING);
    if (esp_wifi_set_config(WIFI_IF_STA, &config) != ESP_OK || esp_wifi_connect() != ESP_OK) {
        set_wifi_state(PLATFORM_NETWORK_FAILED);
        return false;
    }
    return true;
}

bool platform_network_disconnect(void)
{
    if (!atomic_load_explicit(&wifi_initialized, memory_order_acquire)) {
        if (wifi_status_mutex != NULL && xSemaphoreTake(wifi_status_mutex, portMAX_DELAY) == pdTRUE) {
            wifi_connect_pending = false;
            wifi_password[0]     = '\0';
            xSemaphoreGive(wifi_status_mutex);
        }
        set_wifi_state(PLATFORM_NETWORK_OFFLINE);
        return true;
    }
    atomic_store_explicit(&wifi_disconnect_requested, true, memory_order_release);
    const esp_err_t result = esp_wifi_disconnect();
    set_wifi_state(PLATFORM_NETWORK_OFFLINE);
    if (xSemaphoreTake(wifi_status_mutex, portMAX_DELAY) == pdTRUE) {
        wifi_ipv4[0] = '\0';
        xSemaphoreGive(wifi_status_mutex);
    }
    if (result != ESP_OK) {
        atomic_store_explicit(&wifi_disconnect_requested, false, memory_order_release);
    }
    return result == ESP_OK || result == ESP_ERR_WIFI_NOT_CONNECT;
}

bool platform_network_status(platform_network_status_t* status)
{
    if (status == NULL) {
        return false;
    }
    *status = (platform_network_status_t) {
        .state = (platform_network_state_t) atomic_load_explicit(&wifi_state, memory_order_acquire),
    };
    if (wifi_status_mutex != NULL && xSemaphoreTake(wifi_status_mutex, portMAX_DELAY) == pdTRUE) {
        (void) snprintf(status->ssid, sizeof(status->ssid), "%s", wifi_ssid);
        (void) snprintf(status->ipv4, sizeof(status->ipv4), "%s", wifi_ipv4);
        (void) snprintf(status->failure, sizeof(status->failure), "%s", wifi_failure);
        xSemaphoreGive(wifi_status_mutex);
    }
    wifi_ap_record_t access_point;
    if (status->state == PLATFORM_NETWORK_ONLINE && esp_wifi_sta_get_ap_info(&access_point) == ESP_OK) {
        status->signal_dbm = access_point.rssi;
    }
    return true;
}

static uint32_t wifi_security(wifi_auth_mode_t mode)
{
    switch (mode) {
        case WIFI_AUTH_OPEN: return PLATFORM_WIFI_SECURITY_OPEN;
        case WIFI_AUTH_WEP: return PLATFORM_WIFI_SECURITY_WEP;
        case WIFI_AUTH_WPA_PSK: return PLATFORM_WIFI_SECURITY_WPA;
        case WIFI_AUTH_WPA2_PSK: return PLATFORM_WIFI_SECURITY_WPA2;
        case WIFI_AUTH_WPA_WPA2_PSK: return PLATFORM_WIFI_SECURITY_WPA_WPA2;
        case WIFI_AUTH_WPA3_PSK: return PLATFORM_WIFI_SECURITY_WPA3;
        case WIFI_AUTH_WPA2_WPA3_PSK: return PLATFORM_WIFI_SECURITY_WPA2_WPA3;
        case WIFI_AUTH_WPA2_ENTERPRISE:
        case WIFI_AUTH_WPA3_ENT_192: return PLATFORM_WIFI_SECURITY_ENTERPRISE;
        case WIFI_AUTH_OWE: return PLATFORM_WIFI_SECURITY_OWE;
        default: return PLATFORM_WIFI_SECURITY_UNKNOWN;
    }
}

static uint32_t wifi_phy(const wifi_ap_record_t* record)
{
    uint32_t phy = 0U;
    if (record->phy_11b != 0U) {
        phy |= PLATFORM_WIFI_PHY_11B;
    }
    if (record->phy_11g != 0U) {
        phy |= PLATFORM_WIFI_PHY_11G;
    }
    if (record->phy_11n != 0U) {
        phy |= PLATFORM_WIFI_PHY_11N;
    }
    if (record->phy_11ax != 0U) {
        phy |= PLATFORM_WIFI_PHY_11AX;
    }
    return phy;
}

static platform_network_operation_result_t wifi_scan_wait(void)
{
    uint32_t waited_ms = 0U;
    while (xSemaphoreTake(wifi_scan_done, pdMS_TO_TICKS(WIFI_SCAN_POLL_MS)) != pdTRUE) {
        if (platform_riscv32_current_cancelled() || atomic_load_explicit(&wifi_scan_cancel, memory_order_acquire)) {
            return PLATFORM_NETWORK_OPERATION_IO;
        }
        waited_ms += WIFI_SCAN_POLL_MS;
        if (waited_ms >= WIFI_SCAN_TIMEOUT_MS) {
            return PLATFORM_NETWORK_OPERATION_TIMEOUT;
        }
    }
    /* A failed SCAN_DONE is normally an association (esp_wifi_connect) preempting
     * the scan. Report it as busy so callers retry instead of seeing an I/O error. */
    if (atomic_load_explicit(&wifi_scan_failed, memory_order_acquire)) {
        return PLATFORM_NETWORK_OPERATION_BUSY;
    }
    return PLATFORM_NETWORK_OPERATION_OK;
}

static platform_network_operation_result_t wifi_scan_collect(platform_wifi_access_point_t* access_points,
                                                             uint32_t capacity, uint32_t* count)
{
    /* ESP-Hosted 1.4 zero-fills the whole request and never writes the delivered
     * count back to number. Bound the request by the scanned AP count first and
     * discard zero-padding records rather than trusting number afterwards. */
    uint16_t available = 0U;
    if (esp_wifi_scan_get_ap_num(&available) != ESP_OK) {
        return PLATFORM_NETWORK_OPERATION_IO;
    }
    if (available == 0U) {
        *count = 0U;
        return PLATFORM_NETWORK_OPERATION_OK;
    }
    uint16_t number = (uint16_t) capacity;
    if (available < number) {
        number = available;
    }
    const uint32_t requested  = number;
    wifi_ap_record_t* records = calloc(requested, sizeof(*records));
    if (records == NULL) {
        return PLATFORM_NETWORK_OPERATION_IO;
    }
    if (esp_wifi_scan_get_ap_records(&number, records) != ESP_OK) {
        free(records);
        return PLATFORM_NETWORK_OPERATION_IO;
    }
    static const uint8_t empty_bssid[6] = {0};
    uint32_t copied                     = 0U;
    for (uint32_t index = 0U; index < requested && index < number; ++index) {
        if (records[index].primary == 0U && memcmp(records[index].bssid, empty_bssid, sizeof(empty_bssid)) == 0) {
            continue;
        }
        platform_wifi_access_point_t* target = &access_points[copied];
        memset(target, 0, sizeof(*target));
        memcpy(target->ssid, records[index].ssid, sizeof(target->ssid) - 1U);
        memcpy(target->bssid, records[index].bssid, sizeof(target->bssid));
        target->channel    = records[index].primary;
        target->signal_dbm = records[index].rssi;
        target->security   = wifi_security(records[index].authmode);
        target->phy        = wifi_phy(&records[index]);
        ++copied;
    }
    free(records);
    *count = copied;
    return PLATFORM_NETWORK_OPERATION_OK;
}

platform_network_operation_result_t platform_network_scan(platform_wifi_access_point_t* access_points,
                                                          uint32_t capacity, uint32_t* count)
{
    if (access_points == NULL || count == NULL || capacity == 0U) {
        return PLATFORM_NETWORK_OPERATION_INVALID;
    }
    *count = 0U;
    if (capacity > PLATFORM_WIFI_SCAN_MAX) {
        capacity = PLATFORM_WIFI_SCAN_MAX;
    }
    if (wifi_scan_lock == NULL || wifi_scan_done == NULL) {
        return PLATFORM_NETWORK_OPERATION_OFFLINE;
    }
    if (!atomic_load_explicit(&wifi_initialized, memory_order_acquire)) {
        if (tab5_wifi_startup_running(&wifi_startup)) {
            return PLATFORM_NETWORK_OPERATION_BUSY;
        }
        return PLATFORM_NETWORK_OPERATION_OFFLINE;
    }
    if (xSemaphoreTake(wifi_scan_lock, 0) != pdTRUE) {
        return PLATFORM_NETWORK_OPERATION_BUSY;
    }
    if (atomic_load_explicit(&wifi_scan_cancel, memory_order_acquire)) {
        (void) xSemaphoreGive(wifi_scan_lock);
        return PLATFORM_NETWORK_OPERATION_OFFLINE;
    }

    /* Wait for WIFI_EVENT_SCAN_DONE instead of a blocking start so cancellation
     * and the bounded timeout stay under TabOS control across ESP-Hosted. */
    (void) xSemaphoreTake(wifi_scan_done, 0);
    atomic_store_explicit(&wifi_scan_failed, false, memory_order_release);
    atomic_store_explicit(&wifi_scan_active, true, memory_order_release);
    const wifi_scan_config_t config = {
        .show_hidden = true,
        .scan_type   = WIFI_SCAN_TYPE_ACTIVE,
    };
    const esp_err_t started = esp_wifi_scan_start(&config, false);
    platform_network_operation_result_t result = PLATFORM_NETWORK_OPERATION_OK;
    if (started == ESP_ERR_WIFI_STATE) {
        /* The station is associating; esp_wifi_connect owns the radio. */
        result = PLATFORM_NETWORK_OPERATION_BUSY;
    } else if (started != ESP_OK) {
        result = PLATFORM_NETWORK_OPERATION_IO;
    } else {
        result = wifi_scan_wait();
    }
    atomic_store_explicit(&wifi_scan_active, false, memory_order_release);
    if (result == PLATFORM_NETWORK_OPERATION_OK) {
        result = wifi_scan_collect(access_points, capacity, count);
    } else if (started == ESP_OK) {
        (void) esp_wifi_scan_stop();
    }
    (void) xSemaphoreGive(wifi_scan_lock);
    return result;
}

int platform_run(platform_update_fn update, platform_deadline_fn next_deadline)
{
    ESP_LOGI(TAG, "Tab5 platform run loop started");
    atomic_store_explicit(&runtime_task, xTaskGetCurrentTaskHandle(), memory_order_release);
    uint64_t deadline                = platform_time_ms();
    platform_runtime_events_t events = platform_runtime_wait_until(deadline);
    while (!atomic_load_explicit(&stop_requested, memory_order_acquire)) {
        if (update != NULL) {
            update(events);
        }
        if (!atomic_load_explicit(&stop_requested, memory_order_acquire)) {
            deadline = next_deadline != NULL ? next_deadline() : PLATFORM_RUNTIME_DEADLINE_NONE;
            events   = platform_runtime_wait_until(deadline);
        }
    }
    atomic_store_explicit(&runtime_task, NULL, memory_order_release);
    return 0;
}

void platform_stop_run_loop(void)
{
    atomic_store_explicit(&stop_requested, true, memory_order_release);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_SHUTDOWN);
}

void platform_runtime_notify(platform_runtime_events_t events)
{
    if (events == PLATFORM_RUNTIME_EVENT_NONE) {
        return;
    }
    atomic_fetch_or_explicit(&runtime_events, events, memory_order_release);
    TaskHandle_t task = atomic_load_explicit(&runtime_task, memory_order_acquire);
    if (task != NULL) {
        xTaskNotifyGive(task);
    }
}

void platform_runtime_notify_from_isr(platform_runtime_events_t events)
{
    if (events == PLATFORM_RUNTIME_EVENT_NONE) {
        return;
    }
    atomic_fetch_or_explicit(&runtime_events, events, memory_order_release);
    TaskHandle_t task = atomic_load_explicit(&runtime_task, memory_order_acquire);
    if (task != NULL) {
        BaseType_t higher_priority_task_woken = pdFALSE;
        vTaskNotifyGiveFromISR(task, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

platform_runtime_events_t platform_runtime_wait_until(uint64_t deadline_ms)
{
    for (;;) {
        platform_runtime_events_t events = atomic_exchange_explicit(&runtime_events, 0U, memory_order_acq_rel);
        const uint64_t now               = platform_time_ms();
        if (deadline_ms != PLATFORM_RUNTIME_DEADLINE_NONE && now >= deadline_ms) {
            events |= PLATFORM_RUNTIME_EVENT_DEADLINE;
        }
        if (events != PLATFORM_RUNTIME_EVENT_NONE) {
            (void) ulTaskNotifyTake(pdTRUE, 0U);
            return events;
        }
        TickType_t wait_ticks = portMAX_DELAY;
        if (deadline_ms != PLATFORM_RUNTIME_DEADLINE_NONE) {
            const uint64_t remaining_ms    = deadline_ms - now;
            const uint64_t maximum_wait_ms = ((uint64_t) (portMAX_DELAY - 1U) * 1000U) / configTICK_RATE_HZ;
            wait_ticks = remaining_ms >= maximum_wait_ms ? portMAX_DELAY - 1U : pdMS_TO_TICKS(remaining_ms);
            if (wait_ticks == 0U) {
                wait_ticks = 1U;
            }
        }
        (void) ulTaskNotifyTake(pdTRUE, wait_ticks);
    }
}

void platform_shutdown(void)
{
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_SHUTDOWN);
    if (hosted_initialized) {
        (void) esp_hosted_deinit();
        hosted_initialized = false;
    }
    (void) bsp_feature_enable(BSP_FEATURE_WIFI, false);
    platform_display_shutdown();
    tab5_keyboard_shutdown();
    tab5_rtc_shutdown();
}

const char* platform_name(void)
{
    return TABOS_TARGET_NAME_TAB5;
}

bool platform_get_diagnostics(platform_diagnostics_t* diagnostics)
{
    if (diagnostics == NULL) {
        return false;
    }
    uint32_t cpu_frequency_hz = 0U;
    (void) esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_EXACT, &cpu_frequency_hz);
    uint32_t flash_capacity = 0U;
    (void) esp_flash_get_size(NULL, &flash_capacity);
    const bool battery_present = platform_battery_monitor_init();
    *diagnostics               = (platform_diagnostics_t) {
                      .device_name                 = "ESP32-P4",
                      .cpu_cores                   = 2U,
                      .cpu_frequency_mhz           = cpu_frequency_hz / 1000000U,
                      .memory_total_bytes          = heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
                      .memory_free_bytes           = heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                      .memory_free_known           = true,
                      .external_memory_total_bytes = heap_caps_get_total_size(MALLOC_CAP_SPIRAM),
                      .external_memory_free_bytes  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                      .external_memory_present     = true,
                      .flash_capacity_bytes        = flash_capacity,
                      .keyboard_name               = tab5_keyboard_name(),
                      .keyboard_driver             = "Tab5 Keyboard",
                      .keyboard_present            = tab5_keyboard_present(),
                      .keyboard_detected           = tab5_keyboard_detected(),
                      .keyboard_error              = tab5_keyboard_error(),
                      .rtc_name                    = "RX8130",
                      .rtc_present                 = tab5_rtc_present(),
                      .rtc_detected                = tab5_rtc_detected(),
                      .rtc_error                   = tab5_rtc_error(),
                      .battery_name                = "INA226",
                      .battery_present             = battery_present,
                      .battery_detected            = platform_battery_monitor_detected(),
                      .battery_error               = platform_battery_monitor_error(),
                      .network_name                = "ESP32-C6",
                      .network_present             = hosted_initialized,
    };
    return true;
}

void platform_log(const char* message)
{
    if (message != NULL) {
        ESP_LOGI(TABOS_SYSTEM_LOG_TAG, "%s", message);
    }
}

uint64_t platform_time_ms(void)
{
    return (uint64_t) esp_timer_get_time() / 1000U;
}

void platform_input_wait(void)
{
    /* One millisecond rounds to zero at the configured 100 Hz tick rate. */
    vTaskDelay(1);
}
