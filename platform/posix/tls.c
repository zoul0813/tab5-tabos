#include <tabos/platform/platform.h>
#include <tabos/filesystem.h>

#if defined(ESP_PLATFORM)
#include <esp_crt_bundle.h>
#include <esp_tls.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#else
#include <limits.h>
#include <openssl/ssl.h>
#endif

#include <stdio.h>
#include <string.h>

enum {
    TLS_CONNECTION_CAPACITY = 4,
    TLS_HOSTNAME_MAX = 253,
    TLS_IO_MAX = 1024,
};

#if defined(ESP_PLATFORM)
typedef esp_tls_t* native_tls_t;
#else
typedef SSL* native_tls_t;
static SSL_CTX* tls_context;
#endif

typedef struct {
    native_tls_t native;
#if !defined(ESP_PLATFORM)
    BIO* transport;
#endif
} tls_connection_t;

static tls_connection_t connections[TLS_CONNECTION_CAPACITY];

static int tls_error(void)
{
    return -TABOS_EIO;
}

static int connection_allocate(native_tls_t native
#if !defined(ESP_PLATFORM)
                               , BIO* transport
#endif
)
{
    for (int index = 0; index < TLS_CONNECTION_CAPACITY; ++index) {
        if (connections[index].native == NULL) {
            connections[index].native = native;
#if !defined(ESP_PLATFORM)
            connections[index].transport = transport;
#endif
            return index + 1;
        }
    }
    return -TABOS_EMFILE;
}

static native_tls_t connection_get(int connection)
{
    const int index = connection - 1;
    if (index < 0 || index >= TLS_CONNECTION_CAPACITY) {
        return NULL;
    }
    return connections[index].native;
}

#if defined(ESP_PLATFORM)
typedef enum {
    TLS_OPERATION_CONNECT,
    TLS_OPERATION_CLOSE,
    TLS_OPERATION_SEND,
    TLS_OPERATION_RECEIVE,
    TLS_OPERATION_STOP,
} tls_operation_t;

typedef struct {
    tls_operation_t operation;
    int connection;
    uint16_t port;
    uint32_t size;
    char hostname[TLS_HOSTNAME_MAX + 1];
    uint8_t data[TLS_IO_MAX];
} tls_request_t;

typedef struct {
    int result;
    uint8_t data[TLS_IO_MAX];
} tls_response_t;

static QueueHandle_t tls_requests;
static QueueHandle_t tls_responses;
static SemaphoreHandle_t tls_mutex;
static TaskHandle_t tls_task;

static int tls_connect_direct(const char* hostname, uint16_t port)
{
    const esp_tls_cfg_t configuration = {
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };
    esp_tls_t* native = esp_tls_init();
    if (native == NULL || esp_tls_conn_new_sync(hostname, (int) strlen(hostname), (int) port, &configuration, native) != 1) {
        if (native != NULL) {
            (void) esp_tls_conn_destroy(native);
        }
        return tls_error();
    }
    const int result = connection_allocate(native);
    if (result < 0) {
        (void) esp_tls_conn_destroy(native);
    }
    return result;
}

static void tls_worker(void* argument)
{
    (void) argument;
    for (;;) {
        tls_request_t request;
        if (xQueueReceive(tls_requests, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        tls_response_t response = {0};
        if (request.operation == TLS_OPERATION_STOP) {
            for (int index = 0; index < TLS_CONNECTION_CAPACITY; ++index) {
                if (connections[index].native != NULL) {
                    (void) esp_tls_conn_destroy(connections[index].native);
                    connections[index].native = NULL;
                }
            }
            response.result = 0;
            (void) xQueueSend(tls_responses, &response, portMAX_DELAY);
            tls_task = NULL;
            vTaskDelete(NULL);
            return;
        }
        if (request.operation == TLS_OPERATION_CONNECT) {
            response.result = tls_connect_direct(request.hostname, request.port);
        } else {
            native_tls_t native = connection_get(request.connection);
            if (native == NULL) {
                response.result = -TABOS_EBADF;
            } else if (request.operation == TLS_OPERATION_CLOSE) {
                connections[request.connection - 1].native = NULL;
                response.result = esp_tls_conn_destroy(native) == 0 ? 0 : tls_error();
            } else if (request.operation == TLS_OPERATION_SEND) {
                response.result = (int) esp_tls_conn_write(native, request.data, request.size);
                if (response.result <= 0) {
                    response.result = tls_error();
                }
            } else {
                response.result = (int) esp_tls_conn_read(native, response.data, request.size);
                if (response.result < 0) {
                    response.result = tls_error();
                }
            }
        }
        (void) xQueueSend(tls_responses, &response, portMAX_DELAY);
    }
}

bool platform_tls_operations_init(void)
{
    if (tls_task != NULL) {
        return true;
    }
    tls_requests = xQueueCreate(1U, sizeof(tls_request_t));
    tls_responses = xQueueCreate(1U, sizeof(tls_response_t));
    tls_mutex = xSemaphoreCreateMutex();
    if (tls_requests == NULL || tls_responses == NULL || tls_mutex == NULL ||
        xTaskCreate(tls_worker, "tabos_tls", 8192U, NULL, 5U, &tls_task) != pdPASS) {
        if (tls_requests != NULL) { vQueueDelete(tls_requests); }
        if (tls_responses != NULL) { vQueueDelete(tls_responses); }
        if (tls_mutex != NULL) { vSemaphoreDelete(tls_mutex); }
        tls_requests = NULL;
        tls_responses = NULL;
        tls_mutex = NULL;
        tls_task = NULL;
        return false;
    }
    return true;
}

static int submit_request(const tls_request_t* request, tls_response_t* response)
{
    if (!platform_tls_operations_init() || xSemaphoreTake(tls_mutex, portMAX_DELAY) != pdTRUE) {
        return -TABOS_EIO;
    }
    const bool completed = xQueueSend(tls_requests, request, portMAX_DELAY) == pdTRUE &&
                           xQueueReceive(tls_responses, response, portMAX_DELAY) == pdTRUE;
    (void) xSemaphoreGive(tls_mutex);
    return completed ? response->result : -TABOS_EIO;
}

void platform_tls_operations_shutdown(void)
{
    if (tls_task == NULL) {
        return;
    }
    const tls_request_t request = {.operation = TLS_OPERATION_STOP};
    tls_response_t response;
    (void) submit_request(&request, &response);
    vQueueDelete(tls_requests);
    vQueueDelete(tls_responses);
    vSemaphoreDelete(tls_mutex);
    tls_requests = NULL;
    tls_responses = NULL;
    tls_mutex = NULL;
}

int platform_tls_connect(const char* hostname, uint16_t port)
{
    if (hostname == NULL || hostname[0] == '\0' || strnlen(hostname, TLS_HOSTNAME_MAX + 1U) > TLS_HOSTNAME_MAX || port == 0U) {
        return -TABOS_EINVAL;
    }
    tls_request_t request = {.operation = TLS_OPERATION_CONNECT, .port = port};
    (void) snprintf(request.hostname, sizeof(request.hostname), "%s", hostname);
    tls_response_t response;
    return submit_request(&request, &response);
}

int platform_tls_close(int connection)
{
    const tls_request_t request = {.operation = TLS_OPERATION_CLOSE, .connection = connection};
    tls_response_t response;
    return submit_request(&request, &response);
}

int platform_tls_send(int connection, const void* data, uint32_t size)
{
    if (data == NULL || size == 0U || size > TLS_IO_MAX) {
        return -TABOS_EINVAL;
    }
    tls_request_t request = {.operation = TLS_OPERATION_SEND, .connection = connection, .size = size};
    memcpy(request.data, data, size);
    tls_response_t response;
    return submit_request(&request, &response);
}

int platform_tls_receive(int connection, void* data, uint32_t capacity)
{
    if (data == NULL || capacity == 0U || capacity > TLS_IO_MAX) {
        return -TABOS_EINVAL;
    }
    const tls_request_t request = {.operation = TLS_OPERATION_RECEIVE, .connection = connection, .size = capacity};
    tls_response_t response;
    const int result = submit_request(&request, &response);
    if (result > 0) {
        memcpy(data, response.data, (size_t) result);
    }
    return result;
}

#else

bool platform_tls_operations_init(void)
{
    return true;
}

void platform_tls_operations_shutdown(void)
{
    for (int index = 0; index < TLS_CONNECTION_CAPACITY; ++index) {
        if (connections[index].transport != NULL) {
            BIO_free_all(connections[index].transport);
            connections[index].transport = NULL;
            connections[index].native = NULL;
        }
    }
}

int platform_tls_connect(const char* hostname, uint16_t port)
{
    if (hostname == NULL || hostname[0] == '\0' || port == 0U) {
        return -TABOS_EINVAL;
    }
    if (tls_context == NULL) {
        if (OPENSSL_init_ssl(0U, NULL) != 1) {
            return tls_error();
        }
        tls_context = SSL_CTX_new(TLS_client_method());
        if (tls_context == NULL || SSL_CTX_set_default_verify_paths(tls_context) != 1) {
            return tls_error();
        }
        SSL_CTX_set_verify(tls_context, SSL_VERIFY_PEER, NULL);
    }
    BIO* transport = BIO_new_ssl_connect(tls_context);
    if (transport == NULL) {
        return tls_error();
    }
    char endpoint[320];
    (void) snprintf(endpoint, sizeof(endpoint), "%s:%u", hostname, (unsigned int) port);
    BIO_set_conn_hostname(transport, endpoint);
    SSL* native = NULL;
    BIO_get_ssl(transport, &native);
    if (native == NULL || SSL_set_tlsext_host_name(native, hostname) != 1 || SSL_set1_host(native, hostname) != 1 ||
        BIO_do_connect(transport) <= 0 || SSL_get_verify_result(native) != X509_V_OK) {
        BIO_free_all(transport);
        return tls_error();
    }
    const int result = connection_allocate(native, transport);
    if (result < 0) {
        BIO_free_all(transport);
    }
    return result;
}

int platform_tls_close(int connection)
{
    if (connection_get(connection) == NULL) {
        return -TABOS_EBADF;
    }
    BIO* transport = connections[connection - 1].transport;
    connections[connection - 1].native = NULL;
    connections[connection - 1].transport = NULL;
    BIO_free_all(transport);
    return 0;
}

int platform_tls_send(int connection, const void* data, uint32_t size)
{
    native_tls_t native = connection_get(connection);
    if (native == NULL) { return -TABOS_EBADF; }
    if (data == NULL || size == 0U || size > INT_MAX) { return -TABOS_EINVAL; }
    const int result = SSL_write(native, data, (int) size);
    return result > 0 ? result : tls_error();
}

int platform_tls_receive(int connection, void* data, uint32_t capacity)
{
    native_tls_t native = connection_get(connection);
    if (native == NULL) { return -TABOS_EBADF; }
    if (data == NULL || capacity == 0U || capacity > INT_MAX) { return -TABOS_EINVAL; }
    const int result = SSL_read(native, data, (int) capacity);
    return result >= 0 ? result : tls_error();
}

#endif
