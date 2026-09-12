#include <tabos/filesystem.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>

/* Compile the real native TLS loops with deterministic ESP-TLS completion/WANT
 * responses. Certificates and transport are covered separately by host TLS. */
enum {
    ESP_TLS_ERR_SSL_WANT_READ  = -100,
    ESP_TLS_ERR_SSL_WANT_WRITE = -101
};
typedef struct {
        int unused;
} esp_tls_t;
typedef esp_tls_t* native_tls_t;
typedef struct {
        void (*crt_bundle_attach)(void);
        int timeout_ms;
        bool non_block;
} esp_tls_cfg_t;
typedef enum {
    TLS_OPERATION_SEND,
    TLS_OPERATION_RECEIVE
} tls_operation_t;
typedef struct {
        tls_operation_t operation;
        uint32_t size;
        uint8_t data[1024];
} tls_request_t;
typedef struct {
        uint8_t data[1024];
} tls_response_t;

#include "../../platform/posix/native_cancel.h"
static native_cancel_t tls_cancellation;
static uint64_t milliseconds;
static unsigned int cancel_after;
static unsigned int delays;
static unsigned int live_connections;
static int connect_result;
static int transfer_result;
static native_tls_t allocated;

static uint64_t platform_time_ms(void)
{
    return milliseconds;
}
static void vTaskDelay(unsigned int ticks)
{
    (void) ticks;
    milliseconds += 10U;
    if (++delays == cancel_after) {
        native_cancel_request(&tls_cancellation, NULL);
    }
}

static void esp_crt_bundle_attach(void)
{
}
static esp_tls_t* esp_tls_init(void)
{
    ++live_connections;
    return calloc(1U, sizeof(esp_tls_t));
}
static int esp_tls_conn_destroy(esp_tls_t* native)
{
    assert(native != NULL && live_connections > 0U);
    --live_connections;
    free(native);
    return 0;
}
static int esp_tls_conn_new_async(const char* hostname, int length, int port, const esp_tls_cfg_t* config,
                                  esp_tls_t* native)
{
    assert(hostname != NULL && length > 0 && port == 443 && native != NULL);
    assert(config->non_block && config->timeout_ms == 10000 && config->crt_bundle_attach != NULL);
    return connect_result;
}
static int esp_tls_conn_write(esp_tls_t* native, const void* data, uint32_t size)
{
    assert(native != NULL && data != NULL && size > 0U);
    return transfer_result;
}
static int esp_tls_conn_read(esp_tls_t* native, void* data, uint32_t size)
{
    assert(native != NULL && data != NULL && size > 0U);
    return transfer_result;
}
static int tls_error(void)
{
    return -TABOS_EIO;
}
static int connection_allocate(native_tls_t native)
{
    assert(allocated == NULL);
    allocated = native;
    return 1;
}

#include "../../platform/posix/native_tls.inc"

static void reset(void)
{
    milliseconds = 0U;
    cancel_after = 2U;
    delays       = 0U;
    native_cancel_begin(&tls_cancellation, NULL);
}

int main(void)
{
    for (unsigned int round = 0U; round < 20U; ++round) {
        reset();
        connect_result = 0;
        assert(tls_connect_direct("example.test", 443U) == -TABOS_EIO);
        assert(live_connections == 0U && allocated == NULL && delays == 2U);
        reset();
        connect_result = 1;
        assert(tls_connect_direct("example.test", 443U) == 1 && live_connections == 1U);
        tls_request_t request = {.operation = TLS_OPERATION_RECEIVE, .size = 1U};
        tls_response_t response;
        transfer_result = ESP_TLS_ERR_SSL_WANT_READ;
        assert(tls_transfer_direct(allocated, &request, &response) == -TABOS_ECANCELED);
        reset();
        request.operation = TLS_OPERATION_SEND;
        transfer_result   = ESP_TLS_ERR_SSL_WANT_WRITE;
        assert(tls_transfer_direct(allocated, &request, &response) == -TABOS_ECANCELED);
        reset();
        transfer_result = 1;
        assert(tls_transfer_direct(allocated, &request, &response) == 1);
        reset();
        cancel_after    = 0U;
        transfer_result = ESP_TLS_ERR_SSL_WANT_READ;
        assert(tls_transfer_direct(allocated, &request, &response) == -TABOS_EIO && milliseconds == 10000U);
        esp_tls_conn_destroy(allocated);
        allocated = NULL;
    }
    assert(live_connections == 0U);
    return 0;
}
