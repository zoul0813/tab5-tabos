#include "internal.h"

#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>

#include <esp_flash.h>
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
#include <string.h>

static const char* const TAG = TABOS_PLATFORM_LOG_TAG;
static atomic_bool stop_requested;
static bool hosted_initialized;
static atomic_bool wifi_initialized;
static TaskHandle_t wifi_start_task;
static atomic_bool wifi_starting;
static atomic_int wifi_state;
static atomic_bool wifi_disconnect_requested;
static SemaphoreHandle_t wifi_status_mutex;
static bool wifi_connect_pending;
static char wifi_ssid[33];
static char wifi_password[65];
static char wifi_ipv4[16];
static char wifi_failure[64];

static void network_event(void* argument, esp_event_base_t base, int32_t id, void* data)
{
    (void) argument;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t* event = data;
        if (atomic_exchange_explicit(&wifi_disconnect_requested, false, memory_order_acq_rel)) {
            atomic_store_explicit(&wifi_state, PLATFORM_NETWORK_OFFLINE, memory_order_release);
        } else {
            if (xSemaphoreTake(wifi_status_mutex, portMAX_DELAY) == pdTRUE) {
                (void) snprintf(wifi_failure, sizeof(wifi_failure), "Wi-Fi disconnect reason %u",
                                event != NULL ? event->reason : 0U);
                xSemaphoreGive(wifi_status_mutex);
            }
            atomic_store_explicit(&wifi_state, PLATFORM_NETWORK_FAILED, memory_order_release);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t* event = data;
        if (event != NULL && xSemaphoreTake(wifi_status_mutex, portMAX_DELAY) == pdTRUE) {
            (void) snprintf(wifi_ipv4, sizeof(wifi_ipv4), IPSTR, IP2STR(&event->ip_info.ip));
            xSemaphoreGive(wifi_status_mutex);
        }
        atomic_store_explicit(&wifi_state, PLATFORM_NETWORK_ONLINE, memory_order_release);
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
    hosted_initialized = false;
    if (!platform_usb_port_disable_host_power()) {
        ESP_LOGE(TAG, "Could not place USB-A port in safe unpowered state");
        return false;
    }
    (void) tab5_keyboard_init();
    (void) tab5_rtc_init();
    tab5_network_init();
    return true;
}

static bool wifi_driver_init(void)
{
    if (esp_netif_init() != ESP_OK) {
        return false;
    }
    const esp_err_t loop_result = esp_event_loop_create_default();
    if (loop_result != ESP_OK && loop_result != ESP_ERR_INVALID_STATE) {
        return false;
    }
    if (esp_netif_create_default_wifi_sta() == NULL) {
        return false;
    }
    const wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&init) != ESP_OK ||
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, network_event, NULL) != ESP_OK ||
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, network_event, NULL) != ESP_OK ||
        esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK || esp_wifi_start() != ESP_OK) {
        return false;
    }
    atomic_store_explicit(&wifi_state, PLATFORM_NETWORK_OFFLINE, memory_order_release);
    atomic_store_explicit(&wifi_disconnect_requested, false, memory_order_release);
    atomic_store_explicit(&wifi_initialized, true, memory_order_release);
    return true;
}

static void wifi_start(void* argument)
{
    (void) argument;
    if (esp_hosted_slave_reset() != ESP_OK) {
        ESP_LOGW(TAG, "ESP32-C6 hosted transport did not become ready");
        atomic_store_explicit(&wifi_state, PLATFORM_NETWORK_FAILED, memory_order_release);
        atomic_store_explicit(&wifi_starting, false, memory_order_release);
        vTaskDelete(NULL);
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
        atomic_store_explicit(&wifi_state, PLATFORM_NETWORK_FAILED, memory_order_release);
        atomic_store_explicit(&wifi_starting, false, memory_order_release);
        vTaskDelete(NULL);
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
        atomic_store_explicit(&wifi_state, PLATFORM_NETWORK_CONNECTING, memory_order_release);
        if (esp_wifi_set_config(WIFI_IF_STA, &config) != ESP_OK || esp_wifi_connect() != ESP_OK) {
            atomic_store_explicit(&wifi_state, PLATFORM_NETWORK_FAILED, memory_order_release);
        }
    } else {
        atomic_store_explicit(&wifi_state, PLATFORM_NETWORK_OFFLINE, memory_order_release);
    }
    atomic_store_explicit(&wifi_starting, false, memory_order_release);
    vTaskDelete(NULL);
}

bool platform_network_init(void)
{
    if (!hosted_initialized) {
        return false;
    }
    wifi_status_mutex = xSemaphoreCreateMutex();
    if (wifi_status_mutex == NULL) {
        return false;
    }
    wifi_connect_pending = false;
    atomic_store_explicit(&wifi_state, PLATFORM_NETWORK_STARTING, memory_order_release);
    atomic_store_explicit(&wifi_starting, true, memory_order_release);
    if (xTaskCreate(wifi_start, "tabos_wifi_start", 4096U, NULL, 5U, &wifi_start_task) != pdPASS) {
        atomic_store_explicit(&wifi_starting, false, memory_order_release);
        vSemaphoreDelete(wifi_status_mutex);
        wifi_status_mutex = NULL;
        return false;
    }
    return true;
}

void platform_network_shutdown(void)
{
    if (atomic_exchange_explicit(&wifi_starting, false, memory_order_acq_rel)) {
        vTaskDelete(wifi_start_task);
    }
    if (!atomic_load_explicit(&wifi_initialized, memory_order_acquire)) {
        if (wifi_status_mutex != NULL) {
            vSemaphoreDelete(wifi_status_mutex);
            wifi_status_mutex = NULL;
        }
        return;
    }
    (void) esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, network_event);
    (void) esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, network_event);
    (void) esp_wifi_stop();
    (void) esp_wifi_deinit();
    vSemaphoreDelete(wifi_status_mutex);
    wifi_status_mutex = NULL;
    atomic_store_explicit(&wifi_initialized, false, memory_order_release);
}

bool platform_network_connect(const char* ssid, const char* password)
{
    if (!atomic_load_explicit(&wifi_initialized, memory_order_acquire) || ssid == NULL || password == NULL ||
        strlen(ssid) > 32U || strlen(password) > 64U) {
        if (!atomic_load_explicit(&wifi_starting, memory_order_acquire) || ssid == NULL || password == NULL ||
            strlen(ssid) > 32U || strlen(password) > 64U) {
            return false;
        }
        if (xSemaphoreTake(wifi_status_mutex, portMAX_DELAY) == pdTRUE) {
            (void) snprintf(wifi_ssid, sizeof(wifi_ssid), "%s", ssid);
            (void) snprintf(wifi_password, sizeof(wifi_password), "%s", password);
            wifi_connect_pending = true;
            xSemaphoreGive(wifi_status_mutex);
        }
        atomic_store_explicit(&wifi_state, PLATFORM_NETWORK_CONNECTING, memory_order_release);
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
    atomic_store_explicit(&wifi_disconnect_requested, false, memory_order_release);
    atomic_store_explicit(&wifi_state, PLATFORM_NETWORK_CONNECTING, memory_order_release);
    if (esp_wifi_set_config(WIFI_IF_STA, &config) != ESP_OK || esp_wifi_connect() != ESP_OK) {
        atomic_store_explicit(&wifi_state, PLATFORM_NETWORK_FAILED, memory_order_release);
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
        atomic_store_explicit(&wifi_state, PLATFORM_NETWORK_OFFLINE, memory_order_release);
        return true;
    }
    atomic_store_explicit(&wifi_disconnect_requested, true, memory_order_release);
    const esp_err_t result = esp_wifi_disconnect();
    atomic_store_explicit(&wifi_state, PLATFORM_NETWORK_OFFLINE, memory_order_release);
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

int platform_run(platform_update_fn update)
{
    ESP_LOGI(TAG, "Tab5 platform run loop started");
    while (!atomic_load_explicit(&stop_requested, memory_order_acquire)) {
        tab5_keyboard_poll();
        if (update != NULL) {
            update();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return 0;
}

void platform_stop_run_loop(void)
{
    atomic_store_explicit(&stop_requested, true, memory_order_release);
}

void platform_shutdown(void)
{
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
    uint32_t flash_capacity = 0U;
    (void) esp_flash_get_size(NULL, &flash_capacity);
    *diagnostics = (platform_diagnostics_t) {
        .device_name                 = "ESP32-P4",
        .cpu_cores                   = 2U,
        .cpu_frequency_mhz           = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .memory_total_bytes          = heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
        .memory_free_bytes           = heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        .memory_free_known           = true,
        .external_memory_total_bytes = heap_caps_get_total_size(MALLOC_CAP_SPIRAM),
        .external_memory_free_bytes  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        .external_memory_present     = true,
        .flash_capacity_bytes        = flash_capacity,
        .keyboard_name               = tab5_keyboard_name(),
        .keyboard_present            = tab5_keyboard_present(),
        .rtc_name                    = "RX8130",
        .rtc_present                 = tab5_rtc_present(),
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
