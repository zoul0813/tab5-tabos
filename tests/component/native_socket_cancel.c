#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

/* Use real POSIX socket execution beneath the exact native worker loop. */
#define platform_network_socket_operations_cancel host_socket_cancel_unused
#include "../../platform/posix/socket.c"
#undef platform_network_socket_operations_cancel

static void vTaskDelay(unsigned int ticks)
{
    (void) ticks;
    const struct timespec delay = {.tv_nsec = 1000000};
    nanosleep(&delay, NULL);
}

uint64_t platform_time_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t) now.tv_sec * 1000U + (uint64_t) now.tv_nsec / 1000000U;
}

#include "../../platform/posix/native_socket.inc"

static socket_request_t pending_request;
static socket_response_t pending_response;
static atomic_bool completed;

static void* run_request(void* argument)
{
    (void) argument;
    execute_cancellable_socket_request(&pending_request, &pending_response);
    atomic_store(&completed, true);
    return NULL;
}

static void cancel_request(socket_request_t request)
{
    int owner = 0;
    int other = 0;
    native_cancel_begin(&socket_cancellation, &owner);
    atomic_store(&completed, false);
    pending_request = request;
    pthread_t thread;
    assert(pthread_create(&thread, NULL, run_request, NULL) == 0);
    const uint64_t started = platform_time_ms();
    while (platform_time_ms() - started < 20U) {
        vTaskDelay(1U);
    }
    assert(!atomic_load(&completed));
    platform_network_socket_operations_cancel(&other);
    for (unsigned int index = 0U; index < 20U; ++index) {
        vTaskDelay(1U);
    }
    assert(!atomic_load(&completed));
    platform_network_socket_operations_cancel(&owner);
    pthread_join(thread, NULL);
    assert(pending_response.result == -TABOS_ECANCELED);
    assert(platform_time_ms() - started < 500U);
}

int main(void)
{
    int owner = 0;
    int next  = 0;
    native_cancel_begin(&socket_cancellation, &owner);
    platform_network_socket_operations_cancel(&owner);
    assert(native_cancel_pending(&socket_cancellation));
    native_cancel_begin(&socket_cancellation, &next);
    platform_network_socket_operations_cancel(&owner);
    assert(!native_cancel_pending(&socket_cancellation));
    assert(platform_network_socket_operations_init());
    for (unsigned int round = 0U; round < 5U; ++round) {
        const int listener         = socket(AF_INET, SOCK_STREAM, 0);
        const int datagram         = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in address = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
        assert(listener >= 0 && datagram >= 0);
        assert(bind(listener, (struct sockaddr*) &address, sizeof(address)) == 0 && listen(listener, 1) == 0);
        assert(bind(datagram, (struct sockaddr*) &address, sizeof(address)) == 0);
        cancel_request((socket_request_t) {.operation = SOCKET_OPERATION_ACCEPT, .socket = listener});
        cancel_request((socket_request_t) {.operation = SOCKET_OPERATION_RECEIVE_FROM, .socket = datagram, .size = 1U});
        cancel_request((socket_request_t) {.operation  = SOCKET_OPERATION_WAIT,
                                           .wait_count = 1U,
                                           .timeout_ms = UINT32_MAX,
                                           .wait_items = {{.socket = datagram, .events = TABOS_WAIT_READABLE}}});
        assert((fcntl(listener, F_GETFL, 0) & O_NONBLOCK) == 0);
        assert((fcntl(datagram, F_GETFL, 0) & O_NONBLOCK) == 0);
        native_cancel_begin(&socket_cancellation, NULL);
        const socket_request_t poll = {.operation  = SOCKET_OPERATION_WAIT,
                                       .wait_count = 1U,
                                       .wait_items = {{.socket = datagram, .events = TABOS_WAIT_READABLE}}};
        socket_response_t response;
        execute_cancellable_socket_request(&poll, &response);
        assert(response.result == 0); /* No stale cancellation/reply reaches next request. */
        platform_network_socket_operations_cancel(NULL);
        const socket_request_t cleanup = {.operation = SOCKET_OPERATION_CLOSE, .socket = datagram};
        execute_cancellable_socket_request(&cleanup, &response);
        assert(response.result == 0); /* Gate error cleanup must still release handles. */
        close(listener);
    }
    platform_network_socket_operations_shutdown();
    return 0;
}
