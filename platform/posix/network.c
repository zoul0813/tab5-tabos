#include <tabos/platform/platform.h>

#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lwip/inet.h>
#include <lwip/inet_chksum.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>

enum {
    ICMP_HEADER_BYTES = 8,
    ICMP_ECHO_REQUEST = 8,
    ICMP_ECHO_REPLY = 0,
    ICMP6_ECHO_REQUEST = 128,
    ICMP6_ECHO_REPLY = 129,
    ECHO_BUFFER_BYTES = ICMP_HEADER_BYTES + 1024,
};

typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
} icmp_header_t;

#if !defined(ESP_PLATFORM)
static uint16_t checksum(const void* data, size_t size)
{
    const uint8_t* bytes = data;
    uint32_t sum = 0U;
    while (size > 1U) {
        sum += (uint32_t) bytes[0] << 8U | bytes[1];
        bytes += 2;
        size -= 2U;
    }
    if (size != 0U) {
        sum += (uint32_t) bytes[0] << 8U;
    }
    while ((sum >> 16U) != 0U) {
        sum = (sum & UINT32_C(0xffff)) + (sum >> 16U);
    }
    return (uint16_t) ~sum;
}
#endif

static void close_socket(int descriptor)
{
#if defined(ESP_PLATFORM)
    (void) lwip_close(descriptor);
#else
    (void) close(descriptor);
#endif
}

static platform_network_operation_result_t network_resolve_direct(const char* hostname, uint32_t family,
                                                                  platform_network_address_t* address)
{
    if (hostname == NULL || address == NULL || hostname[0] == '\0' ||
        (family != 0U && family != 4U && family != 6U)) {
        return PLATFORM_NETWORK_OPERATION_INVALID;
    }
    const struct addrinfo hints = {
        .ai_family = family == 4U ? AF_INET : family == 6U ? AF_INET6 : AF_UNSPEC,
        .ai_socktype = SOCK_DGRAM,
    };
    struct addrinfo* addresses = NULL;
    const int resolved = getaddrinfo(hostname, NULL, &hints, &addresses);
    if (resolved != 0 || addresses == NULL) {
        return PLATFORM_NETWORK_OPERATION_NOT_FOUND;
    }
    const struct addrinfo* selected = addresses;
    while (selected != NULL && selected->ai_family != AF_INET && selected->ai_family != AF_INET6) {
        selected = selected->ai_next;
    }
    if (selected == NULL) {
        freeaddrinfo(addresses);
        return PLATFORM_NETWORK_OPERATION_NOT_FOUND;
    }
    const void* binary = selected->ai_family == AF_INET
                             ? (const void*) &((const struct sockaddr_in*) selected->ai_addr)->sin_addr
                             : (const void*) &((const struct sockaddr_in6*) selected->ai_addr)->sin6_addr;
    memset(address, 0, sizeof(*address));
    address->family = selected->ai_family == AF_INET ? 4U : 6U;
    const char* converted = inet_ntop(selected->ai_family, binary, address->text, sizeof(address->text));
    freeaddrinfo(addresses);
    return converted != NULL ? PLATFORM_NETWORK_OPERATION_OK : PLATFORM_NETWORK_OPERATION_IO;
}

static int open_icmp_socket(int family, int protocol)
{
#if defined(ESP_PLATFORM)
    /* ESP-IDF's supported ICMP implementation uses raw sockets. */
    return socket(family, SOCK_RAW, protocol);
#else
    int descriptor = socket(family, SOCK_DGRAM, protocol);
    if (descriptor < 0) {
        descriptor = socket(family, SOCK_RAW, protocol);
    }
    return descriptor;
#endif
}

static platform_network_operation_result_t network_echo_direct(const platform_network_address_t* address,
                                                               uint16_t sequence, uint16_t payload_bytes,
                                                               uint32_t timeout_ms,
                                                               platform_network_echo_result_t* result)
{
    if (address == NULL || result == NULL || (address->family != 4U && address->family != 6U) ||
        address->text[0] == '\0' || payload_bytes > 1024U || timeout_ms == 0U) {
        return PLATFORM_NETWORK_OPERATION_INVALID;
    }
    const int family = address->family == 4U ? AF_INET : AF_INET6;
    const int protocol = address->family == 4U ? IPPROTO_ICMP : IPPROTO_ICMPV6;
    int descriptor = open_icmp_socket(family, protocol);
    if (descriptor < 0) {
        char message[96];
        (void) snprintf(message, sizeof(message), "ICMP socket failed for %s: errno %d", address->text, errno);
        platform_log(message);
        return errno == EACCES || errno == EPERM ? PLATFORM_NETWORK_OPERATION_UNSUPPORTED
                                                 : PLATFORM_NETWORK_OPERATION_IO;
    }
    struct sockaddr_storage target;
    memset(&target, 0, sizeof(target));
    socklen_t target_size;
    if (family == AF_INET) {
        struct sockaddr_in* ipv4 = (struct sockaddr_in*) &target;
        ipv4->sin_family = AF_INET;
        target_size = sizeof(*ipv4);
        if (inet_pton(AF_INET, address->text, &ipv4->sin_addr) != 1) {
            close_socket(descriptor);
            return PLATFORM_NETWORK_OPERATION_INVALID;
        }
    } else {
        struct sockaddr_in6* ipv6 = (struct sockaddr_in6*) &target;
        ipv6->sin6_family = AF_INET6;
        target_size = sizeof(*ipv6);
        if (inet_pton(AF_INET6, address->text, &ipv6->sin6_addr) != 1) {
            close_socket(descriptor);
            return PLATFORM_NETWORK_OPERATION_INVALID;
        }
    }
    uint8_t packet[ECHO_BUFFER_BYTES];
    const size_t packet_size = ICMP_HEADER_BYTES + payload_bytes;
    memset(packet, 0, packet_size);
    icmp_header_t* header = (icmp_header_t*) packet;
    header->type = family == AF_INET ? ICMP_ECHO_REQUEST : ICMP6_ECHO_REQUEST;
    header->identifier = htons(UINT16_C(0x544f));
    header->sequence = htons(sequence);
    for (size_t index = ICMP_HEADER_BYTES; index < packet_size; ++index) {
        packet[index] = (uint8_t) index;
    }
    if (family == AF_INET) {
#if defined(ESP_PLATFORM)
        header->checksum = inet_chksum(packet, packet_size);
#else
        header->checksum = htons(checksum(packet, packet_size));
#endif
    }
    const uint64_t started = platform_time_ms();
    if (sendto(descriptor, packet, packet_size, 0, (const struct sockaddr*) &target, target_size) < 0) {
        char message[96];
        (void) snprintf(message, sizeof(message), "ICMP send failed for %s: errno %d", address->text, errno);
        platform_log(message);
        close_socket(descriptor);
        return PLATFORM_NETWORK_OPERATION_IO;
    }
    for (;;) {
        const uint64_t elapsed = platform_time_ms() - started;
        if (elapsed >= timeout_ms) {
            close_socket(descriptor);
            return PLATFORM_NETWORK_OPERATION_TIMEOUT;
        }
        const uint32_t remaining_ms = timeout_ms - (uint32_t) elapsed;
        const struct timeval timeout = {
            .tv_sec = (long) (remaining_ms / 1000U),
            .tv_usec = (int) ((remaining_ms % 1000U) * 1000U),
        };
        if (setsockopt(descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
            close_socket(descriptor);
            return PLATFORM_NETWORK_OPERATION_IO;
        }
        struct sockaddr_storage source;
        socklen_t source_size = sizeof(source);
        const ssize_t received = recvfrom(descriptor, packet, sizeof(packet), 0, (struct sockaddr*) &source, &source_size);
        if (received < 0) {
            const int failure = errno;
            char message[96];
            (void) snprintf(message, sizeof(message), "ICMP receive ended for %s: errno %d", address->text, failure);
            platform_log(message);
            close_socket(descriptor);
            return failure == EAGAIN || failure == EWOULDBLOCK || failure == ETIMEDOUT
                       ? PLATFORM_NETWORK_OPERATION_TIMEOUT
                       : PLATFORM_NETWORK_OPERATION_IO;
        }
        size_t offset = 0U;
        if (family == AF_INET && received >= 20 && (packet[0] >> 4U) == 4U) {
            offset = (size_t) (packet[0] & 0x0fU) * 4U;
        }
        if ((size_t) received < offset + ICMP_HEADER_BYTES) {
            continue;
        }
        const icmp_header_t* reply = (const icmp_header_t*) (packet + offset);
        const uint8_t expected = family == AF_INET ? ICMP_ECHO_REPLY : ICMP6_ECHO_REPLY;
        const bool correct_source =
            family == AF_INET
                ? memcmp(&((const struct sockaddr_in*) &source)->sin_addr,
                         &((const struct sockaddr_in*) &target)->sin_addr, sizeof(struct in_addr)) == 0
                : memcmp(&((const struct sockaddr_in6*) &source)->sin6_addr,
                         &((const struct sockaddr_in6*) &target)->sin6_addr, sizeof(struct in6_addr)) == 0;
        if (!correct_source || reply->type != expected || ntohs(reply->sequence) != sequence) {
            continue;
        }
        close_socket(descriptor);
        result->sequence = sequence;
        result->bytes = (uint32_t) received - (uint32_t) offset - ICMP_HEADER_BYTES;
        const uint64_t final_elapsed = platform_time_ms() - started;
        result->round_trip_ms = final_elapsed > UINT32_MAX ? UINT32_MAX : (uint32_t) final_elapsed;
        return PLATFORM_NETWORK_OPERATION_OK;
    }
}

#if defined(ESP_PLATFORM)

/*
 * Mapped RV32 applications use the hardware thread pointer for guest TLS.
 * ESP-IDF's lwIP calls use native pthread TLS, so every lwIP operation must run
 * on a native FreeRTOS task rather than directly inside an application API call.
 */
typedef enum {
    NETWORK_WORKER_RESOLVE,
    NETWORK_WORKER_ECHO,
    NETWORK_WORKER_STOP,
} network_worker_operation_t;

typedef struct {
    network_worker_operation_t operation;
    char hostname[254];
    uint32_t family;
    platform_network_address_t address;
    uint16_t sequence;
    uint16_t payload_bytes;
    uint32_t timeout_ms;
} network_worker_request_t;

typedef struct {
    platform_network_operation_result_t operation;
    platform_network_address_t address;
    platform_network_echo_result_t echo;
} network_worker_response_t;

static QueueHandle_t worker_requests;
static QueueHandle_t worker_responses;
static SemaphoreHandle_t worker_mutex;
static TaskHandle_t worker_task;

static void network_worker(void* argument)
{
    (void) argument;
    for (;;) {
        network_worker_request_t request;
        if (xQueueReceive(worker_requests, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        network_worker_response_t response = {0};
        if (request.operation == NETWORK_WORKER_STOP) {
            response.operation = PLATFORM_NETWORK_OPERATION_OK;
            (void) xQueueSend(worker_responses, &response, portMAX_DELAY);
            worker_task = NULL;
            vTaskDelete(NULL);
            return;
        }
        if (request.operation == NETWORK_WORKER_RESOLVE) {
            response.operation = network_resolve_direct(request.hostname, request.family, &response.address);
        } else {
            response.operation = network_echo_direct(&request.address, request.sequence, request.payload_bytes,
                                                     request.timeout_ms, &response.echo);
        }
        (void) xQueueSend(worker_responses, &response, portMAX_DELAY);
    }
}

bool platform_network_operations_init(void)
{
    if (worker_task != NULL) {
        return true;
    }
    worker_requests = xQueueCreate(1U, sizeof(network_worker_request_t));
    worker_responses = xQueueCreate(1U, sizeof(network_worker_response_t));
    worker_mutex = xSemaphoreCreateMutex();
    if (worker_requests == NULL || worker_responses == NULL || worker_mutex == NULL ||
        xTaskCreate(network_worker, "tabos_net_ops", 4096U, NULL, 5U, &worker_task) != pdPASS) {
        if (worker_requests != NULL) {
            vQueueDelete(worker_requests);
        }
        if (worker_responses != NULL) {
            vQueueDelete(worker_responses);
        }
        if (worker_mutex != NULL) {
            vSemaphoreDelete(worker_mutex);
        }
        worker_requests = NULL;
        worker_responses = NULL;
        worker_mutex = NULL;
        worker_task = NULL;
        return false;
    }
    return true;
}

static platform_network_operation_result_t submit_request(const network_worker_request_t* request,
                                                          network_worker_response_t* response)
{
    if (worker_task == NULL || request == NULL || response == NULL ||
        xSemaphoreTake(worker_mutex, portMAX_DELAY) != pdTRUE) {
        return PLATFORM_NETWORK_OPERATION_IO;
    }
    const bool completed = xQueueSend(worker_requests, request, portMAX_DELAY) == pdTRUE &&
                           xQueueReceive(worker_responses, response, portMAX_DELAY) == pdTRUE;
    (void) xSemaphoreGive(worker_mutex);
    return completed ? response->operation : PLATFORM_NETWORK_OPERATION_IO;
}

void platform_network_operations_shutdown(void)
{
    if (worker_task == NULL) {
        return;
    }
    const network_worker_request_t request = {.operation = NETWORK_WORKER_STOP};
    network_worker_response_t response;
    (void) submit_request(&request, &response);
    vQueueDelete(worker_requests);
    vQueueDelete(worker_responses);
    vSemaphoreDelete(worker_mutex);
    worker_requests = NULL;
    worker_responses = NULL;
    worker_mutex = NULL;
}

platform_network_operation_result_t platform_network_resolve(const char* hostname, uint32_t family,
                                                             platform_network_address_t* address)
{
    if (hostname == NULL || address == NULL || strnlen(hostname, 254U) == 254U) {
        return PLATFORM_NETWORK_OPERATION_INVALID;
    }
    network_worker_request_t request = {.operation = NETWORK_WORKER_RESOLVE, .family = family};
    (void) snprintf(request.hostname, sizeof(request.hostname), "%s", hostname);
    network_worker_response_t response;
    const platform_network_operation_result_t operation = submit_request(&request, &response);
    if (operation == PLATFORM_NETWORK_OPERATION_OK) {
        *address = response.address;
    }
    return operation;
}

platform_network_operation_result_t platform_network_echo(const platform_network_address_t* address,
                                                          uint16_t sequence, uint16_t payload_bytes,
                                                          uint32_t timeout_ms,
                                                          platform_network_echo_result_t* result)
{
    if (address == NULL || result == NULL) {
        return PLATFORM_NETWORK_OPERATION_INVALID;
    }
    const network_worker_request_t request = {
        .operation = NETWORK_WORKER_ECHO,
        .address = *address,
        .sequence = sequence,
        .payload_bytes = payload_bytes,
        .timeout_ms = timeout_ms,
    };
    network_worker_response_t response;
    const platform_network_operation_result_t operation = submit_request(&request, &response);
    if (operation == PLATFORM_NETWORK_OPERATION_OK) {
        *result = response.echo;
    }
    return operation;
}

#else

bool platform_network_operations_init(void)
{
    return true;
}

void platform_network_operations_shutdown(void)
{
}

platform_network_operation_result_t platform_network_resolve(const char* hostname, uint32_t family,
                                                             platform_network_address_t* address)
{
    return network_resolve_direct(hostname, family, address);
}

platform_network_operation_result_t platform_network_echo(const platform_network_address_t* address,
                                                          uint16_t sequence, uint16_t payload_bytes,
                                                          uint32_t timeout_ms,
                                                          platform_network_echo_result_t* result)
{
    return network_echo_direct(address, sequence, payload_bytes, timeout_ms, result);
}

#endif
