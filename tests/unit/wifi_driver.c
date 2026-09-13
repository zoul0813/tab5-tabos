#define _POSIX_C_SOURCE 200809L
#include "wifi_driver.h"
#include "wifi_startup.h"

#include <esp_wifi.h>

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct esp_netif {
        int unused;
};

enum {
    STEP_NETIF_INIT = 1,
    STEP_EVENT_LOOP,
    STEP_CREATE_NETIF,
    STEP_HOSTNAME,
    STEP_WIFI_INIT,
    STEP_WIFI_HANDLER,
    STEP_IP_HANDLER,
    STEP_WIFI_MODE,
    STEP_WIFI_START,
    STEP_COUNT,
};

static struct esp_netif fake_netif;
static int fail_step;
static int current_step;
static bool shared_event_loop;
static unsigned int event_loop_deletes;
static unsigned int netif_destroys;
static unsigned int wifi_deinits;
static unsigned int wifi_stops;
static unsigned int handler_unregisters;

struct fake_semaphore {
        pthread_mutex_t mutex;
        pthread_cond_t changed;
        bool given;
};

SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
    SemaphoreHandle_t semaphore = calloc(1U, sizeof(*semaphore));
    assert(semaphore != NULL);
    assert(pthread_mutex_init(&semaphore->mutex, NULL) == 0);
    assert(pthread_cond_init(&semaphore->changed, NULL) == 0);
    return semaphore;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore)
{
    assert(pthread_mutex_lock(&semaphore->mutex) == 0);
    semaphore->given = true;
    assert(pthread_cond_broadcast(&semaphore->changed) == 0);
    assert(pthread_mutex_unlock(&semaphore->mutex) == 0);
    return pdTRUE;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t timeout)
{
    assert(timeout == portMAX_DELAY);
    assert(pthread_mutex_lock(&semaphore->mutex) == 0);
    while (!semaphore->given) {
        assert(pthread_cond_wait(&semaphore->changed, &semaphore->mutex) == 0);
    }
    semaphore->given = false;
    assert(pthread_mutex_unlock(&semaphore->mutex) == 0);
    return pdTRUE;
}

void vSemaphoreDelete(SemaphoreHandle_t semaphore)
{
    assert(pthread_cond_destroy(&semaphore->changed) == 0);
    assert(pthread_mutex_destroy(&semaphore->mutex) == 0);
    free(semaphore);
}

static esp_err_t step_result(int step)
{
    current_step = step;
    return fail_step == step ? ESP_FAIL : ESP_OK;
}

esp_err_t esp_netif_init(void)
{
    return step_result(STEP_NETIF_INIT);
}

esp_err_t esp_event_loop_create_default(void)
{
    if (shared_event_loop) {
        current_step = STEP_EVENT_LOOP;
        return ESP_ERR_INVALID_STATE;
    }
    return step_result(STEP_EVENT_LOOP);
}

esp_err_t esp_event_loop_delete_default(void)
{
    ++event_loop_deletes;
    return ESP_OK;
}

esp_netif_t* esp_netif_create_default_wifi_sta(void)
{
    current_step = STEP_CREATE_NETIF;
    return fail_step == STEP_CREATE_NETIF ? NULL : &fake_netif;
}

esp_err_t esp_netif_set_hostname(esp_netif_t* netif, const char* hostname)
{
    assert(netif == &fake_netif);
    assert(strcmp(hostname, "tabos-test") == 0);
    return step_result(STEP_HOSTNAME);
}

void esp_netif_destroy_default_wifi(void* netif)
{
    assert(netif == &fake_netif);
    ++netif_destroys;
}

esp_err_t esp_wifi_init(const wifi_init_config_t* config)
{
    assert(config != NULL);
    return step_result(STEP_WIFI_INIT);
}

esp_err_t esp_wifi_deinit(void)
{
    ++wifi_deinits;
    return ESP_OK;
}

esp_err_t esp_event_handler_instance_register(esp_event_base_t base, int32_t id, esp_event_handler_t handler,
                                              void* argument, esp_event_handler_instance_t* instance)
{
    assert(handler != NULL && argument == NULL);
    const int step = strcmp(base, WIFI_EVENT) == 0 ? STEP_WIFI_HANDLER : STEP_IP_HANDLER;
    assert((step == STEP_WIFI_HANDLER && id == ESP_EVENT_ANY_ID) ||
           (step == STEP_IP_HANDLER && id == IP_EVENT_STA_GOT_IP));
    if (step_result(step) != ESP_OK) {
        return ESP_FAIL;
    }
    *instance = (void*) (uintptr_t) step;
    return ESP_OK;
}

esp_err_t esp_event_handler_instance_unregister(esp_event_base_t base, int32_t id,
                                                esp_event_handler_instance_t instance)
{
    (void) base;
    (void) id;
    assert(instance != NULL);
    ++handler_unregisters;
    return ESP_OK;
}

esp_err_t esp_wifi_set_mode(int mode)
{
    assert(mode == WIFI_MODE_STA);
    return step_result(STEP_WIFI_MODE);
}

esp_err_t esp_wifi_start(void)
{
    return step_result(STEP_WIFI_START);
}

esp_err_t esp_wifi_stop(void)
{
    ++wifi_stops;
    return ESP_OK;
}

static void event_handler(void* argument, esp_event_base_t base, int32_t id, void* data)
{
    (void) argument;
    (void) base;
    (void) id;
    (void) data;
}

static void reset_fakes(void)
{
    fail_step           = 0;
    current_step        = 0;
    shared_event_loop   = false;
    event_loop_deletes  = 0U;
    netif_destroys      = 0U;
    wifi_deinits        = 0U;
    wifi_stops          = 0U;
    handler_unregisters = 0U;
}

static void assert_empty(const tab5_wifi_driver_t* driver)
{
    assert(driver->netif == NULL);
    assert(driver->wifi_handler == NULL);
    assert(driver->ip_handler == NULL);
    assert(!driver->event_loop_owned);
    assert(!driver->wifi_initialized);
    assert(!driver->wifi_started);
}

static void test_failure_unwind(void)
{
    for (fail_step = STEP_NETIF_INIT; fail_step < STEP_COUNT; ++fail_step) {
        const int requested_failure = fail_step;
        tab5_wifi_driver_t driver   = {0};
        event_loop_deletes          = 0U;
        netif_destroys              = 0U;
        wifi_deinits                = 0U;
        wifi_stops                  = 0U;
        handler_unregisters         = 0U;
        assert(!tab5_wifi_driver_init(&driver, "tabos-test", event_handler));
        assert(current_step == requested_failure);
        assert_empty(&driver);
        assert(event_loop_deletes == (requested_failure > STEP_EVENT_LOOP ? 1U : 0U));
        assert(netif_destroys == (requested_failure > STEP_CREATE_NETIF ? 1U : 0U));
        assert(wifi_deinits == (requested_failure > STEP_WIFI_INIT ? 1U : 0U));
        assert(handler_unregisters == (requested_failure > STEP_IP_HANDLER   ? 2U :
                                       requested_failure > STEP_WIFI_HANDLER ? 1U :
                                                                               0U));
        assert(wifi_stops == (requested_failure == STEP_WIFI_START ? 1U : 0U));
    }
}

static void test_success_shutdown(void)
{
    reset_fakes();
    tab5_wifi_driver_t driver = {0};
    assert(tab5_wifi_driver_init(&driver, "tabos-test", event_handler));
    tab5_wifi_driver_shutdown(&driver);
    assert_empty(&driver);
    assert(event_loop_deletes == 1U);
    assert(netif_destroys == 1U);
    assert(wifi_deinits == 1U);
    assert(wifi_stops == 1U);
    assert(handler_unregisters == 2U);
    tab5_wifi_driver_shutdown(&driver);
    assert_empty(&driver);
}

static void test_shared_infrastructure(void)
{
    reset_fakes();
    shared_event_loop         = true;
    tab5_wifi_driver_t driver = {0};
    assert(tab5_wifi_driver_init(&driver, "tabos-test", event_handler));
    tab5_wifi_driver_shutdown(&driver);
    assert(event_loop_deletes == 0U);
    assert(netif_destroys == 1U);
    assert(wifi_deinits == 1U);
    assert(wifi_stops == 1U);
    assert(handler_unregisters == 2U);
}

static atomic_bool startup_resource_in_use;

static void* startup_worker(void* argument)
{
    tab5_wifi_startup_t* startup = argument;
    atomic_store(&startup_resource_in_use, true);
    while (!tab5_wifi_startup_cancelled(startup)) {
        const struct timespec delay = {.tv_nsec = 1000000};
        nanosleep(&delay, NULL);
    }
    atomic_store(&startup_resource_in_use, false);
    tab5_wifi_startup_finish(startup);
    return NULL;
}

static void test_startup_shutdown_join(void)
{
    tab5_wifi_startup_t startup = {0};
    assert(tab5_wifi_startup_prepare(&startup));
    assert(tab5_wifi_startup_running(&startup));
    atomic_store(&startup_resource_in_use, false);
    pthread_t worker;
    assert(pthread_create(&worker, NULL, startup_worker, &startup) == 0);
    while (!atomic_load(&startup_resource_in_use)) {
        const struct timespec delay = {.tv_nsec = 1000000};
        nanosleep(&delay, NULL);
    }
    tab5_wifi_startup_stop_and_join(&startup);
    assert(!atomic_load(&startup_resource_in_use));
    assert(!tab5_wifi_startup_running(&startup));
    assert(pthread_join(worker, NULL) == 0);
    tab5_wifi_startup_stop_and_join(&startup);
}

int main(void)
{
    reset_fakes();
    test_failure_unwind();
    test_success_shutdown();
    test_shared_infrastructure();
    test_startup_shutdown_join();
    puts("wifi driver tests passed");
    return 0;
}
