#include <tabos/filesystem.h>
#include <tabos/network.h>
#include <tabos/platform/platform.h>

#if defined(ESP_PLATFORM)
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <errno.h>
#include <string.h>

typedef enum {
    SOCKET_OPERATION_OPEN,
    SOCKET_OPERATION_CLOSE,
    SOCKET_OPERATION_BIND,
    SOCKET_OPERATION_LOCAL_ENDPOINT,
    SOCKET_OPERATION_LISTEN,
    SOCKET_OPERATION_ACCEPT,
    SOCKET_OPERATION_CONNECT,
    SOCKET_OPERATION_NONBLOCKING,
    SOCKET_OPERATION_SHUTDOWN,
    SOCKET_OPERATION_SEND,
    SOCKET_OPERATION_RECEIVE,
    SOCKET_OPERATION_SEND_TO,
    SOCKET_OPERATION_RECEIVE_FROM,
} socket_operation_t;

typedef struct {
        socket_operation_t operation;
        int socket;
        uint32_t family;
        uint32_t type;
        platform_network_address_t address;
        uint16_t port;
        uint16_t backlog;
        uint32_t direction;
        uint32_t size;
        bool enabled;
        uint8_t data[TABOS_NETWORK_IO_MAX];
} socket_request_t;

typedef struct {
        int result;
        platform_network_address_t address;
        uint16_t port;
        uint8_t data[TABOS_NETWORK_IO_MAX];
} socket_response_t;

static int socket_error(void)
{
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS) {
        return -TABOS_EAGAIN;
    }
    if (errno == EBADF) {
        return -TABOS_EBADF;
    }
    if (errno == EINVAL) {
        return -TABOS_EINVAL;
    }
    if (errno == EACCES || errno == EPERM) {
        return -TABOS_EACCES;
    }
    if (errno == ENOMEM || errno == ENOBUFS) {
        return -TABOS_ENOMEM;
    }
    if (errno == ETIMEDOUT) {
        return -TABOS_ETIMEDOUT;
    }
    if (errno == ENETDOWN || errno == ENETUNREACH) {
        return -TABOS_ENETDOWN;
    }
#if defined(ESP_PLATFORM)
    ESP_LOGE("tabos_socket", "lwIP socket error: errno=%d (%s)", errno, strerror(errno));
#endif
    return -TABOS_EIO;
}

static int close_native_socket(int descriptor)
{
#if defined(ESP_PLATFORM)
    return lwip_close(descriptor);
#else
    return close(descriptor);
#endif
}

static bool native_endpoint(const platform_network_address_t* address, uint16_t port, struct sockaddr_storage* storage,
                            socklen_t* size)
{
    if (address == NULL || storage == NULL || size == NULL) {
        return false;
    }
    memset(storage, 0, sizeof(*storage));
    if (address->family == 4U) {
        struct sockaddr_in* ipv4 = (struct sockaddr_in*) storage;
        ipv4->sin_family         = AF_INET;
        ipv4->sin_port           = htons(port);
        *size                    = sizeof(*ipv4);
        return inet_pton(AF_INET, address->text, &ipv4->sin_addr) == 1;
    }
    if (address->family == 6U) {
        struct sockaddr_in6* ipv6 = (struct sockaddr_in6*) storage;
        ipv6->sin6_family         = AF_INET6;
        ipv6->sin6_port           = htons(port);
        *size                     = sizeof(*ipv6);
        return inet_pton(AF_INET6, address->text, &ipv6->sin6_addr) == 1;
    }
    return false;
}

static bool portable_endpoint(const struct sockaddr_storage* storage, platform_network_address_t* address,
                              uint16_t* port)
{
    if (storage == NULL || address == NULL || port == NULL) {
        return false;
    }
    memset(address, 0, sizeof(*address));
    if (storage->ss_family == AF_INET) {
        const struct sockaddr_in* ipv4 = (const struct sockaddr_in*) storage;
        address->family                = 4U;
        *port                          = ntohs(ipv4->sin_port);
        return inet_ntop(AF_INET, &ipv4->sin_addr, address->text, sizeof(address->text)) != NULL;
    }
    if (storage->ss_family == AF_INET6) {
        const struct sockaddr_in6* ipv6 = (const struct sockaddr_in6*) storage;
        address->family                 = 6U;
        *port                           = ntohs(ipv6->sin6_port);
        return inet_ntop(AF_INET6, &ipv6->sin6_addr, address->text, sizeof(address->text)) != NULL;
    }
    return false;
}

static void execute_socket_request(const socket_request_t* request, socket_response_t* response)
{
    memset(response, 0, sizeof(*response));
    if (request->operation == SOCKET_OPERATION_OPEN) {
        const int family = request->family == 4U ? AF_INET : request->family == 6U ? AF_INET6 : -1;
        const int type   = request->type == TABOS_SOCKET_TCP ? SOCK_STREAM :
                           request->type == TABOS_SOCKET_UDP ? SOCK_DGRAM :
                                                               -1;
        if (family < 0 || type < 0) {
            response->result = -TABOS_EINVAL;
            return;
        }
        const int descriptor = socket(family, type, 0);
        response->result     = descriptor >= 0 ? descriptor : socket_error();
        return;
    }
    if (request->operation == SOCKET_OPERATION_CLOSE) {
        response->result = close_native_socket(request->socket) == 0 ? 0 : socket_error();
        return;
    }
    if (request->operation == SOCKET_OPERATION_LISTEN) {
        response->result = listen(request->socket, request->backlog) == 0 ? 0 : socket_error();
        return;
    }
    if (request->operation == SOCKET_OPERATION_NONBLOCKING) {
        const int current = fcntl(request->socket, F_GETFL, 0);
        if (current < 0) {
            response->result = socket_error();
            return;
        }
        int flags = current & ~O_NONBLOCK;
        if (request->enabled) {
            flags |= O_NONBLOCK;
        }
        response->result = fcntl(request->socket, F_SETFL, flags) == 0 ? 0 : socket_error();
        return;
    }
    if (request->operation == SOCKET_OPERATION_SHUTDOWN) {
        response->result = shutdown(request->socket, (int) request->direction) == 0 ? 0 : socket_error();
        return;
    }
    if (request->operation == SOCKET_OPERATION_SEND) {
        const ssize_t sent = send(request->socket, request->data, request->size, 0);
        response->result   = sent >= 0 ? (int) sent : socket_error();
        return;
    }
    if (request->operation == SOCKET_OPERATION_RECEIVE) {
        const ssize_t received = recv(request->socket, response->data, request->size, 0);
        response->result       = received >= 0 ? (int) received : socket_error();
        return;
    }
    struct sockaddr_storage endpoint;
    socklen_t endpoint_size = sizeof(endpoint);
    if (request->operation == SOCKET_OPERATION_ACCEPT) {
        const int accepted = accept(request->socket, (struct sockaddr*) &endpoint, &endpoint_size);
        if (accepted < 0) {
            response->result = socket_error();
            return;
        }
        if (!portable_endpoint(&endpoint, &response->address, &response->port)) {
            (void) close_native_socket(accepted);
            response->result = -TABOS_EIO;
            return;
        }
        response->result = accepted;
        return;
    }
    if (request->operation == SOCKET_OPERATION_LOCAL_ENDPOINT) {
        if (getsockname(request->socket, (struct sockaddr*) &endpoint, &endpoint_size) != 0) {
            response->result = socket_error();
            return;
        }
        response->result = portable_endpoint(&endpoint, &response->address, &response->port) ? 0 : -TABOS_EIO;
        return;
    }
    if (request->operation == SOCKET_OPERATION_RECEIVE_FROM) {
        const ssize_t received =
            recvfrom(request->socket, response->data, request->size, 0, (struct sockaddr*) &endpoint, &endpoint_size);
        if (received < 0) {
            response->result = socket_error();
            return;
        }
        response->result =
            portable_endpoint(&endpoint, &response->address, &response->port) ? (int) received : -TABOS_EIO;
        return;
    }
    if (!native_endpoint(&request->address, request->port, &endpoint, &endpoint_size)) {
        response->result = -TABOS_EINVAL;
        return;
    }
    if (request->operation == SOCKET_OPERATION_BIND) {
        response->result =
            bind(request->socket, (const struct sockaddr*) &endpoint, endpoint_size) == 0 ? 0 : socket_error();
    } else if (request->operation == SOCKET_OPERATION_CONNECT) {
        response->result =
            connect(request->socket, (const struct sockaddr*) &endpoint, endpoint_size) == 0 ? 0 : socket_error();
    } else {
        const ssize_t sent =
            sendto(request->socket, request->data, request->size, 0, (const struct sockaddr*) &endpoint, endpoint_size);
        response->result = sent >= 0 ? (int) sent : socket_error();
    }
}

#if defined(ESP_PLATFORM)
static QueueHandle_t socket_requests;
static QueueHandle_t socket_responses;
static SemaphoreHandle_t socket_mutex;
static TaskHandle_t socket_task;

static void socket_worker(void* argument)
{
    (void) argument;
    for (;;) {
        socket_request_t request;
        if (xQueueReceive(socket_requests, &request, portMAX_DELAY) == pdTRUE) {
            socket_response_t response;
            execute_socket_request(&request, &response);
            (void) xQueueSend(socket_responses, &response, portMAX_DELAY);
        }
    }
}

static bool socket_worker_init(void)
{
    if (socket_task != NULL) {
        return true;
    }
    socket_requests  = xQueueCreate(1U, sizeof(socket_request_t));
    socket_responses = xQueueCreate(1U, sizeof(socket_response_t));
    socket_mutex     = xSemaphoreCreateMutex();
    if (socket_requests != NULL && socket_responses != NULL && socket_mutex != NULL &&
        xTaskCreate(socket_worker, "tabos_sockets", 6144U, NULL, 5U, &socket_task) == pdPASS) {
        return true;
    }
    if (socket_requests != NULL) {
        vQueueDelete(socket_requests);
        socket_requests = NULL;
    }
    if (socket_responses != NULL) {
        vQueueDelete(socket_responses);
        socket_responses = NULL;
    }
    if (socket_mutex != NULL) {
        vSemaphoreDelete(socket_mutex);
        socket_mutex = NULL;
    }
    socket_task = NULL;
    return false;
}

bool platform_network_socket_operations_init(void)
{
    return socket_worker_init();
}

void platform_network_socket_operations_shutdown(void)
{
    if (socket_task != NULL) {
        vTaskDelete(socket_task);
        socket_task = NULL;
    }
    if (socket_requests != NULL) {
        vQueueDelete(socket_requests);
        socket_requests = NULL;
    }
    if (socket_responses != NULL) {
        vQueueDelete(socket_responses);
        socket_responses = NULL;
    }
    if (socket_mutex != NULL) {
        vSemaphoreDelete(socket_mutex);
        socket_mutex = NULL;
    }
}

void platform_network_socket_interrupt(int socket)
{
    (void) shutdown(socket, SHUT_RDWR);
}

bool platform_network_socket_operations_suspend(void)
{
    if (!socket_worker_init()) {
        return false;
    }
    return xSemaphoreTake(socket_mutex, portMAX_DELAY) == pdTRUE;
}

void platform_network_socket_operations_resume(void)
{
    if (socket_mutex != NULL) {
        (void) xSemaphoreGive(socket_mutex);
    }
}

void platform_network_socket_dispose(int socket)
{
    (void) close_native_socket(socket);
}

static int submit_socket_request(const socket_request_t* request, socket_response_t* response)
{
    if (!socket_worker_init()) {
        ESP_LOGE("tabos_socket", "worker initialization failed for operation %u", (unsigned int) request->operation);
        return -TABOS_EIO;
    }
    if (xSemaphoreTake(socket_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE("tabos_socket", "worker mutex failed for operation %u", (unsigned int) request->operation);
        return -TABOS_EIO;
    }
    const bool sent     = xQueueSend(socket_requests, request, portMAX_DELAY) == pdTRUE;
    const bool received = sent && xQueueReceive(socket_responses, response, portMAX_DELAY) == pdTRUE;
    (void) xSemaphoreGive(socket_mutex);
    if (!sent || !received) {
        ESP_LOGE("tabos_socket", "worker queue failed for operation %u: sent=%u received=%u",
                 (unsigned int) request->operation, sent ? 1U : 0U, received ? 1U : 0U);
        return -TABOS_EIO;
    }
    return response->result;
}
#else
bool platform_network_socket_operations_init(void)
{
    return true;
}

void platform_network_socket_operations_shutdown(void)
{
}

void platform_network_socket_interrupt(int socket)
{
    (void) shutdown(socket, SHUT_RDWR);
}

bool platform_network_socket_operations_suspend(void)
{
    return true;
}

void platform_network_socket_operations_resume(void)
{
}

void platform_network_socket_dispose(int socket)
{
    (void) close_native_socket(socket);
}

static int submit_socket_request(const socket_request_t* request, socket_response_t* response)
{
    execute_socket_request(request, response);
    return response->result;
}
#endif

static int simple_request(socket_operation_t operation, int socket)
{
    const socket_request_t request = {.operation = operation, .socket = socket};
    socket_response_t response;
    return submit_socket_request(&request, &response);
}

int platform_network_socket_open(uint32_t family, uint32_t type)
{
    const socket_request_t request = {.operation = SOCKET_OPERATION_OPEN, .family = family, .type = type};
    socket_response_t response;
    return submit_socket_request(&request, &response);
}

int platform_network_socket_close(int socket)
{
    return simple_request(SOCKET_OPERATION_CLOSE, socket);
}

static int endpoint_request(socket_operation_t operation, int socket, const platform_network_address_t* address,
                            uint16_t port)
{
    if (address == NULL) {
        return -TABOS_EINVAL;
    }
    const socket_request_t request = {.operation = operation, .socket = socket, .address = *address, .port = port};
    socket_response_t response;
    return submit_socket_request(&request, &response);
}

int platform_network_socket_bind(int socket, const platform_network_address_t* address, uint16_t port)
{
    return endpoint_request(SOCKET_OPERATION_BIND, socket, address, port);
}

int platform_network_socket_get_local_endpoint(int socket, platform_network_address_t* address, uint16_t* port)
{
    if (address == NULL || port == NULL) {
        return -TABOS_EINVAL;
    }
    const socket_request_t request = {.operation = SOCKET_OPERATION_LOCAL_ENDPOINT, .socket = socket};
    socket_response_t response;
    const int result = submit_socket_request(&request, &response);
    if (result == 0) {
        *address = response.address;
        *port    = response.port;
    }
    return result;
}

int platform_network_socket_listen(int socket, uint16_t backlog)
{
    const socket_request_t request = {.operation = SOCKET_OPERATION_LISTEN, .socket = socket, .backlog = backlog};
    socket_response_t response;
    return submit_socket_request(&request, &response);
}

int platform_network_socket_accept(int socket, platform_network_address_t* address, uint16_t* port)
{
    const socket_request_t request = {.operation = SOCKET_OPERATION_ACCEPT, .socket = socket};
    socket_response_t response;
    const int accepted = submit_socket_request(&request, &response);
    if (accepted >= 0 && address != NULL && port != NULL) {
        *address = response.address;
        *port    = response.port;
    }
    return accepted;
}

int platform_network_socket_connect(int socket, const platform_network_address_t* address, uint16_t port)
{
    return endpoint_request(SOCKET_OPERATION_CONNECT, socket, address, port);
}

int platform_network_socket_set_nonblocking(int socket, bool enabled)
{
    const socket_request_t request = {
        .operation = SOCKET_OPERATION_NONBLOCKING,
        .socket    = socket,
        .enabled   = enabled,
    };
    socket_response_t response;
    return submit_socket_request(&request, &response);
}

int platform_network_socket_shutdown(int socket, uint32_t direction)
{
    const socket_request_t request = {.operation = SOCKET_OPERATION_SHUTDOWN, .socket = socket, .direction = direction};
    socket_response_t response;
    return submit_socket_request(&request, &response);
}

static int data_request(socket_operation_t operation, int socket, const void* data, uint32_t size,
                        const platform_network_address_t* address, uint16_t port, socket_response_t* response)
{
    if (size == 0U || size > TABOS_NETWORK_IO_MAX ||
        ((operation == SOCKET_OPERATION_SEND || operation == SOCKET_OPERATION_SEND_TO) && data == NULL)) {
        return -TABOS_EINVAL;
    }
    socket_request_t request = {.operation = operation, .socket = socket, .size = size, .port = port};
    if (data != NULL) {
        memcpy(request.data, data, size);
    }
    if (address != NULL) {
        request.address = *address;
    }
    return submit_socket_request(&request, response);
}

int platform_network_socket_send(int socket, const void* data, uint32_t size)
{
    socket_response_t response;
    return data_request(SOCKET_OPERATION_SEND, socket, data, size, NULL, 0U, &response);
}

int platform_network_socket_receive(int socket, void* data, uint32_t capacity)
{
    socket_response_t response;
    const int received = data_request(SOCKET_OPERATION_RECEIVE, socket, NULL, capacity, NULL, 0U, &response);
    if (received > 0) {
        memcpy(data, response.data, (size_t) received);
    }
    return received;
}

int platform_network_socket_send_to(int socket, const void* data, uint32_t size,
                                    const platform_network_address_t* address, uint16_t port)
{
    socket_response_t response;
    return data_request(SOCKET_OPERATION_SEND_TO, socket, data, size, address, port, &response);
}

int platform_network_socket_receive_from(int socket, void* data, uint32_t capacity, platform_network_address_t* address,
                                         uint16_t* port)
{
    socket_response_t response;
    const int received = data_request(SOCKET_OPERATION_RECEIVE_FROM, socket, NULL, capacity, NULL, 0U, &response);
    if (received >= 0) {
        if (received > 0) {
            memcpy(data, response.data, (size_t) received);
        }
        if (address != NULL && port != NULL) {
            *address = response.address;
            *port    = response.port;
        }
    }
    return received;
}
