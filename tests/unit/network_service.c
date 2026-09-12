#include <tabos/internal/filesystem.h>
#include <tabos/internal/network.h>

#include "platform_test.h"

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>

static int failures;

enum {
    CONCURRENT_OPERATION_COUNT   = 2000,
    CONCURRENT_OBSERVATION_COUNT = 8000,
};

static const char concurrent_ssid_a[] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
static const char concurrent_ssid_b[] = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";

typedef struct {
        atomic_bool start;
        atomic_bool failed;
} concurrency_context_t;

static void expect(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static void wait_for_concurrency_start(concurrency_context_t* context)
{
    while (!atomic_load_explicit(&context->start, memory_order_acquire)) {
        sched_yield();
    }
}

static void* connect_concurrently(void* argument)
{
    concurrency_context_t* context = argument;
    wait_for_concurrency_start(context);
    for (unsigned int index = 0U; index < CONCURRENT_OPERATION_COUNT; ++index) {
        const char* ssid = index % 2U == 0U ? concurrent_ssid_a : concurrent_ssid_b;
        if (!network_service_connect(ssid, "concurrent-password", false)) {
            atomic_store_explicit(&context->failed, true, memory_order_release);
        }
    }
    return NULL;
}

static void* disconnect_concurrently(void* argument)
{
    concurrency_context_t* context = argument;
    wait_for_concurrency_start(context);
    for (unsigned int index = 0U; index < CONCURRENT_OPERATION_COUNT; ++index) {
        if (!network_service_disconnect()) {
            atomic_store_explicit(&context->failed, true, memory_order_release);
        }
    }
    return NULL;
}

static void* update_concurrently(void* argument)
{
    concurrency_context_t* context = argument;
    wait_for_concurrency_start(context);
    for (unsigned int index = 0U; index < CONCURRENT_OBSERVATION_COUNT; ++index) {
        network_service_update();
    }
    return NULL;
}

static void* observe_concurrently(void* argument)
{
    concurrency_context_t* context = argument;
    wait_for_concurrency_start(context);
    for (unsigned int index = 0U; index < CONCURRENT_OBSERVATION_COUNT; ++index) {
        network_status_t status;
        if (!network_service_status(&status) || memchr(status.hostname, '\0', sizeof(status.hostname)) == NULL ||
            memchr(status.ssid, '\0', sizeof(status.ssid)) == NULL ||
            memchr(status.ipv4, '\0', sizeof(status.ipv4)) == NULL ||
            memchr(status.last_failure, '\0', sizeof(status.last_failure)) == NULL || status.attempts > 1U ||
            (status.ssid[0] != '\0' && strcmp(status.ssid, "test-network") != 0 &&
             strcmp(status.ssid, concurrent_ssid_a) != 0 && strcmp(status.ssid, concurrent_ssid_b) != 0) ||
            network_service_next_deadline() != UINT64_MAX) {
            atomic_store_explicit(&context->failed, true, memory_order_release);
        }
    }
    return NULL;
}

static void test_concurrent_access(void)
{
    concurrency_context_t context = {0};
    pthread_t workers[4];
    void* (*worker_functions[])(void*) = {
        connect_concurrently,
        disconnect_concurrently,
        update_concurrently,
        observe_concurrently,
    };
    size_t created = 0U;
    while (created < sizeof(workers) / sizeof(workers[0]) &&
           pthread_create(&workers[created], NULL, worker_functions[created], &context) == 0) {
        ++created;
    }
    expect(created == sizeof(workers) / sizeof(workers[0]), "concurrent network workers start");

    atomic_store_explicit(&context.start, true, memory_order_release);
    bool joined = true;
    for (size_t index = 0U; index < created; ++index) {
        if (pthread_join(workers[index], NULL) != 0) {
            joined = false;
        }
    }
    expect(joined, "concurrent network workers finish");
    expect(!atomic_load_explicit(&context.failed, memory_order_acquire),
           "concurrent status, connect, disconnect, update, and deadline access stays consistent");
}

int main(void)
{
    expect(filesystem_init(), "filesystem initializes");
    expect(network_service_init(), "network service initializes");
    expect(strcmp(test_platform_network_hostname(), "TabOS") == 0, "hostname defaults to TabOS");
    network_status_t status;
    expect(network_service_status(&status) && status.state == NETWORK_STATE_OFFLINE &&
               strcmp(status.hostname, "TabOS") == 0,
           "starts offline with default hostname");
    network_service_update();
    const unsigned int initial_status_calls = test_platform_network_status_calls();
    network_service_update();
    network_service_update();
    expect(test_platform_network_status_calls() == initial_status_calls,
           "idle updates do not poll platform network status");

    expect(network_service_connect("test-network", "secret", false), "explicit connect starts");
    expect(test_platform_network_connect_calls() == 1U, "first attempt issued");
    test_platform_network_set_state(PLATFORM_NETWORK_FAILED, "test failure");
    network_service_update();
    expect(test_platform_network_status_calls() == initial_status_calls + 1U,
           "network notification causes one platform status read");
    const uint64_t first_retry_ms = test_platform_time_ms() + 1000U;
    expect(network_service_next_deadline() == first_retry_ms, "retry publishes exact deadline");
    test_platform_advance_time_ms(999U);
    network_service_update();
    expect(test_platform_network_connect_calls() == 1U, "retry does not fire early");
    test_platform_advance_time_ms(1U);
    network_service_update();
    expect(test_platform_network_connect_calls() == 2U, "second attempt issued");
    expect(network_service_next_deadline() == UINT64_MAX, "fired retry clears deadline");
    test_platform_network_set_state(PLATFORM_NETWORK_FAILED, "test failure");
    network_service_update();
    test_platform_advance_time_ms(1000U);
    network_service_update();
    expect(test_platform_network_connect_calls() == 3U, "third attempt issued");
    test_platform_network_set_state(PLATFORM_NETWORK_FAILED, "final failure");
    network_service_update();
    test_platform_advance_time_ms(5000U);
    network_service_update();
    expect(test_platform_network_connect_calls() == 3U, "retry limit enforced");
    expect(network_service_status(&status) && status.state == NETWORK_STATE_FAILED && status.attempts == 3U &&
               strcmp(status.last_failure, "final failure") == 0,
           "final failure retained");

    expect(network_service_connect("test-network", "secret", false), "explicit reconnect resets attempts");
    test_platform_network_set_state(PLATFORM_NETWORK_ONLINE, NULL);
    network_service_update();
    expect(network_service_status(&status) && status.state == NETWORK_STATE_ONLINE &&
               strcmp(status.ipv4, "192.0.2.10") == 0 && status.signal_dbm == -42,
           "online status propagated");
    expect(network_service_next_deadline() == UINT64_MAX, "online state has no retry deadline");
    network_address_t address;
    expect(network_service_resolve("localhost", 4U, &address) == NETWORK_OPERATION_OK && address.family == 4U &&
               strcmp(address.text, "127.0.0.1") == 0,
           "online resolver delegates to platform");
    expect(network_service_resolve("missing.test", 4U, &address) == NETWORK_OPERATION_NOT_FOUND,
           "resolver preserves deterministic not-found result");
    network_echo_result_t echo;
    expect(network_service_echo(&address, 7U, 56U, 1000U, &echo) == NETWORK_OPERATION_OK && echo.sequence == 7U &&
               echo.bytes == 56U && echo.round_trip_ms == 2U,
           "online echo delegates to platform");
    address = (network_address_t) {.family = 4U};
    (void) snprintf(address.text, sizeof(address.text), "%s", "198.51.100.1");
    expect(network_service_echo(&address, 8U, 56U, 1000U, &echo) == NETWORK_OPERATION_TIMEOUT,
           "echo preserves deterministic timeout result");
    expect(network_service_disconnect(), "explicit disconnect succeeds");
    expect(network_service_next_deadline() == UINT64_MAX, "disconnect cancels retry deadline");
    expect(network_service_resolve("localhost", 4U, &address) == NETWORK_OPERATION_OFFLINE,
           "resolver rejects offline operation");
    test_platform_network_set_state(PLATFORM_NETWORK_FAILED, "late failure");
    network_service_update();
    test_platform_advance_time_ms(5000U);
    network_service_update();
    expect(network_service_status(&status) && status.state == NETWORK_STATE_OFFLINE,
           "disconnect suppresses late failure and retry");
    expect(network_service_connect("test-network", "secret", false), "explicit reconnect after disconnect starts");
    test_platform_network_set_state(PLATFORM_NETWORK_ONLINE, NULL);
    network_service_update();
    expect(test_platform_network_connect_calls() == 5U && network_service_status(&status) &&
               status.state == NETWORK_STATE_ONLINE,
           "reconnect after disconnect restores online state");

    expect(network_service_connect("test-network", "secret", false), "connect for retry cancellation starts");
    test_platform_network_set_state(PLATFORM_NETWORK_FAILED, "cancelled failure");
    network_service_update();
    expect(network_service_next_deadline() != UINT64_MAX, "failed connection schedules cancellable retry");
    expect(network_service_disconnect(), "disconnect cancels scheduled retry");
    expect(network_service_next_deadline() == UINT64_MAX, "scheduled retry deadline is cancelled");
    test_platform_advance_time_ms(5000U);
    network_service_update();
    expect(test_platform_network_connect_calls() == 6U, "cancelled retry does not start later");

    test_concurrent_access();

    network_service_shutdown();
    filesystem_shutdown();
    return failures == 0 ? 0 : 1;
}
