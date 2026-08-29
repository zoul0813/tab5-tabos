#include <tabos/internal/filesystem.h>
#include <tabos/internal/network.h>

#include "platform_test.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
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

    expect(network_service_connect("test-network", "secret", false), "explicit connect starts");
    expect(test_platform_network_connect_calls() == 1U, "first attempt issued");
    test_platform_network_set_state(PLATFORM_NETWORK_FAILED, "test failure");
    network_service_update();
    test_platform_advance_time_ms(1000U);
    network_service_update();
    expect(test_platform_network_connect_calls() == 2U, "second attempt issued");
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
    network_address_t address;
    expect(network_service_resolve("localhost", 4U, &address) == NETWORK_OPERATION_OK &&
               address.family == 4U && strcmp(address.text, "127.0.0.1") == 0,
           "online resolver delegates to platform");
    network_echo_result_t echo;
    expect(network_service_echo(&address, 7U, 56U, 1000U, &echo) == NETWORK_OPERATION_OK &&
               echo.sequence == 7U && echo.bytes == 56U && echo.round_trip_ms == 2U,
           "online echo delegates to platform");
    expect(network_service_disconnect(), "explicit disconnect succeeds");
    expect(network_service_resolve("localhost", 4U, &address) == NETWORK_OPERATION_OFFLINE,
           "resolver rejects offline operation");
    test_platform_network_set_state(PLATFORM_NETWORK_FAILED, "late failure");
    network_service_update();
    test_platform_advance_time_ms(5000U);
    network_service_update();
    expect(network_service_status(&status) && status.state == NETWORK_STATE_OFFLINE,
           "disconnect suppresses late failure and retry");

    network_service_shutdown();
    filesystem_shutdown();
    return failures == 0 ? 0 : 1;
}
