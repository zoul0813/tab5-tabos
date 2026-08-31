#include <tester/test.h>

#include <tabos/device.h>
#include <tabos/network.h>
#include <tabos/process.h>
#include <tabos/runtime_time.h>
#include <tabos/wait.h>

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static tabos_socket_endpoint_t loopback_endpoint(void)
{
    tabos_socket_endpoint_t endpoint = {
        .address.family = TABOS_NETWORK_FAMILY_IPV4,
    };
    (void) strcpy(endpoint.address.text, "127.0.0.1");
    return endpoint;
}

static void test_tcp(tester_context_t* context)
{
    tabos_socket_endpoint_t endpoint = loopback_endpoint();
    const tabos_socket_t server      = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_TCP);
    tester_expect(context, server >= 0, "TCP server socket opens");
    tester_expect(context, tabos_socket_bind(server, &endpoint) == 0, "TCP server binds loopback");
    tester_expect(context, tabos_socket_get_local_endpoint(server, &endpoint) == 0 && endpoint.port != 0U,
                  "TCP ephemeral port assigned");
    tester_expect(context, tabos_socket_listen(server, 1U) == 0, "TCP server listens");

    const tabos_socket_t client = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_TCP);
    tester_expect(context, client >= 0 && tabos_socket_connect(client, &endpoint) == 0, "TCP client connects");
    tabos_socket_endpoint_t peer;
    const tabos_socket_t accepted = tabos_socket_accept(server, &peer);
    tester_expect(context, accepted >= 0 && peer.port != 0U, "TCP server accepts client");

    char buffer[32] = {0};
    tester_expect(context,
                  tabos_socket_set_nonblocking(accepted, true) == 0 &&
                      tabos_socket_receive(accepted, buffer, sizeof(buffer)) < 0 && errno == EAGAIN &&
                      tabos_socket_set_nonblocking(accepted, false) == 0,
                  "TCP nonblocking receive returns EAGAIN");
    static const char message[] = "tabos-tcp";
    tabos_wait_item_t wait_item = {
        .source = tabos_socket_wait_source(accepted),
        .events = TABOS_WAIT_READABLE,
    };
    tester_expect(context, tabos_wait(&wait_item, 1U, 0U) == 0 && wait_item.returned_events == 0U,
                  "zero-time wait reports no unread TCP data");
    const bool sent = tabos_socket_send(client, message, sizeof(message)) == (int) sizeof(message);
    const int ready = tabos_wait(&wait_item, 1U, 1000U);
    tester_expect(context,
                  sent && ready == 1 && (wait_item.returned_events & TABOS_WAIT_READABLE) != 0U &&
                      tabos_socket_receive(accepted, buffer, sizeof(buffer)) == (int) sizeof(message) &&
                      memcmp(buffer, message, sizeof(message)) == 0,
                  "finite wait reports readable TCP data");
    tester_expect(context,
                  tabos_socket_close(accepted) == 0 && tabos_socket_close(client) == 0 &&
                      tabos_socket_close(server) == 0,
                  "TCP sockets close");
}

static void test_udp(tester_context_t* context)
{
    tabos_socket_endpoint_t endpoint = loopback_endpoint();
    const tabos_socket_t server      = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_UDP);
    tester_expect(context,
                  server >= 0 && tabos_socket_bind(server, &endpoint) == 0 &&
                      tabos_socket_get_local_endpoint(server, &endpoint) == 0 && endpoint.port != 0U,
                  "UDP server binds ephemeral loopback port");
    const tabos_socket_t client = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_UDP);
    static const char request[] = "tabos-udp";
    tester_expect(context,
                  client >= 0 &&
                      tabos_socket_send_to(client, request, sizeof(request), &endpoint) == (int) sizeof(request),
                  "UDP client sends datagram");
    char buffer[32] = {0};
    tabos_socket_endpoint_t peer;
    tester_expect(context,
                  tabos_socket_receive_from(server, buffer, sizeof(buffer), &peer) == (int) sizeof(request) &&
                      memcmp(buffer, request, sizeof(request)) == 0,
                  "UDP server receives datagram and peer");
    tester_expect(context,
                  tabos_socket_send_to(server, request, sizeof(request), &peer) == (int) sizeof(request) &&
                      tabos_socket_receive(client, buffer, sizeof(buffer)) == (int) sizeof(request),
                  "UDP server replies to peer");
    tester_expect(context, tabos_socket_close(client) == 0 && tabos_socket_close(server) == 0, "UDP sockets close");
}

static void test_stale_handle(tester_context_t* context)
{
    const tabos_socket_t stale             = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_UDP);
    const tabos_wait_source_t stale_source = tabos_socket_wait_source(stale);
    tester_expect(context, stale >= 0 && stale_source != TABOS_WAIT_SOURCE_INVALID && tabos_socket_close(stale) == 0,
                  "socket and wait source close before slot reuse");

    tabos_wait_item_t stale_item = {
        .source = stale_source,
        .events = TABOS_WAIT_READABLE,
    };
    errno                       = 0;
    const int stale_wait_result = tabos_wait(&stale_item, 1U, 0U);
    tester_expect(context, stale_wait_result < 0 && errno == EBADF, "closed socket invalidates its wait source");

    const tabos_socket_t replacement             = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_UDP);
    const tabos_wait_source_t replacement_source = tabos_socket_wait_source(replacement);
    errno                                        = 0;
    const int stale_result                       = tabos_socket_close(stale);
    tester_expect(context,
                  replacement >= 0 && replacement != stale && replacement_source != stale_source && stale_result < 0 &&
                      errno == EBADF,
                  "stale socket and wait-source handles cannot identify replacements");
    if (replacement >= 0) {
        tester_expect(context, tabos_socket_close(replacement) == 0, "replacement socket closes");
    }
}

static void close_sockets(tabos_socket_t* sockets, unsigned int count)
{
    for (unsigned int index = 0U; index < count; ++index) {
        if (sockets[index] >= 0) {
            (void) tabos_socket_close(sockets[index]);
            sockets[index] = -1;
        }
    }
}

static void test_socket_capacity(tester_context_t* context)
{
    tabos_socket_t sockets[TABOS_SOCKET_MAX];
    for (unsigned int index = 0U; index < TABOS_SOCKET_MAX; ++index) {
        sockets[index] = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_UDP);
    }

    bool capacity_opened = true;
    for (unsigned int index = 0U; index < TABOS_SOCKET_MAX; ++index) {
        if (sockets[index] < 0) {
            capacity_opened = false;
        }
    }
    tester_expect(context, capacity_opened, "per-process socket capacity opens");

    errno                       = 0;
    const tabos_socket_t excess = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_UDP);
    tester_expect(context, excess < 0 && errno == EMFILE, "socket table exhaustion returns EMFILE");
    if (excess >= 0) {
        (void) tabos_socket_close(excess);
    }

    const tabos_socket_t stale = sockets[0];
    if (stale >= 0) {
        (void) tabos_socket_close(stale);
        sockets[0] = -1;
    }
    const tabos_socket_t replacement = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_UDP);
    errno                            = 0;
    const int stale_result           = tabos_socket_close(stale);
    tester_expect(context, replacement >= 0 && replacement != stale && stale_result < 0 && errno == EBADF,
                  "one freed socket slot is reusable without reviving stale handle");
    if (replacement >= 0) {
        (void) tabos_socket_close(replacement);
    }
    close_sockets(sockets, TABOS_SOCKET_MAX);
}

static void test_full_table_accept(tester_context_t* context)
{
    tabos_socket_endpoint_t endpoint = loopback_endpoint();
    const tabos_socket_t server      = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_TCP);
    const bool server_ready          = server >= 0 && tabos_socket_bind(server, &endpoint) == 0 &&
                              tabos_socket_get_local_endpoint(server, &endpoint) == 0 && endpoint.port != 0U &&
                              tabos_socket_listen(server, 1U) == 0;
    tester_expect(context, server_ready, "full-table accept server starts");

    tabos_socket_t fillers[TABOS_SOCKET_MAX - 2U];
    for (unsigned int index = 0U; index < TABOS_SOCKET_MAX - 2U; ++index) {
        fillers[index] = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_UDP);
    }
    bool fillers_opened = true;
    for (unsigned int index = 0U; index < TABOS_SOCKET_MAX - 2U; ++index) {
        if (fillers[index] < 0) {
            fillers_opened = false;
        }
    }
    tester_expect(context, fillers_opened, "accept test fills reserved socket slots");

    bool accepted_sockets_released = server_ready && fillers_opened;
    for (unsigned int attempt = 0U; attempt < 4U && accepted_sockets_released; ++attempt) {
        const tabos_socket_t client = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_TCP);
        if (client < 0 || tabos_socket_connect(client, &endpoint) != 0) {
            accepted_sockets_released = false;
        } else {
            errno                         = 0;
            const tabos_socket_t accepted = tabos_socket_accept(server, NULL);
            if (accepted >= 0 || errno != EMFILE) {
                accepted_sockets_released = false;
            }
            if (accepted >= 0) {
                (void) tabos_socket_close(accepted);
            }
        }
        if (client >= 0) {
            (void) tabos_socket_close(client);
        }
    }
    tester_expect(context, accepted_sockets_released,
                  "full-table accept returns EMFILE and releases native accepted sockets");

    close_sockets(fillers, TABOS_SOCKET_MAX - 2U);
    if (server >= 0) {
        (void) tabos_socket_close(server);
    }
}

static int run_fixture(const char* operation)
{
    const char* const arguments[] = {
        "T:/bin/tester",
        operation,
        NULL,
    };
    return tabos_exec(arguments[0], 2, arguments);
}

static void test_wait_sources(tester_context_t* context)
{
    const tabos_device_subscription_t subscription = tabos_device_subscribe();
    const tabos_wait_source_t device_source        = tabos_device_subscription_wait_source(subscription);
    tester_expect(context,
                  subscription != TABOS_DEVICE_SUBSCRIPTION_INVALID && device_source != TABOS_WAIT_SOURCE_INVALID,
                  "device lifecycle wait source opens");

    tabos_wait_item_t device_item = {
        .source = device_source,
        .events = TABOS_WAIT_READABLE | TABOS_WAIT_STATE_CHANGED,
    };
    const uint64_t timeout_started = tabos_monotonic_ms();
    const int timeout_result       = tabos_wait(&device_item, 1U, 20U);
    const uint64_t timeout_elapsed = tabos_monotonic_ms() - timeout_started;
    tester_expect(context, timeout_result == 0 && device_item.returned_events == 0U && timeout_elapsed >= 5U,
                  "finite device wait reaches monotonic timeout without spurious readiness");

    char source_text[16];
    (void) snprintf(source_text, sizeof(source_text), "%" PRId32, device_source);
    const char* const foreign_arguments[] = {
        "T:/bin/tester",
        "--foreign-wait-source",
        source_text,
        NULL,
    };
    tester_expect(context, tabos_exec(foreign_arguments[0], 3, foreign_arguments) == 79,
                  "child process rejects parent wait source");

    tabos_socket_endpoint_t endpoint = loopback_endpoint();
    const tabos_socket_t receiver    = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_UDP);
    const tabos_socket_t sender      = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_UDP);
    const bool sockets_ready         = receiver >= 0 && sender >= 0 && tabos_socket_bind(receiver, &endpoint) == 0 &&
                               tabos_socket_get_local_endpoint(receiver, &endpoint) == 0 && endpoint.port != 0U;
    tester_expect(context, sockets_ready, "mixed-wait UDP sockets open");

    static const char payload[] = "wait-source";
    const bool payload_sent =
        sockets_ready && tabos_socket_send_to(sender, payload, sizeof(payload), &endpoint) == (int) sizeof(payload);
    tabos_wait_item_t mixed_items[] = {
        {                     .source = device_source, .events = TABOS_WAIT_READABLE | TABOS_WAIT_STATE_CHANGED},
        {.source = tabos_socket_wait_source(receiver),                            .events = TABOS_WAIT_READABLE},
    };
    const int mixed_ready = payload_sent ? tabos_wait(mixed_items, 2U, 1000U) : -1;
    tester_expect(context,
                  mixed_ready == 1 && mixed_items[0].returned_events == 0U &&
                      (mixed_items[1].returned_events & TABOS_WAIT_READABLE) != 0U,
                  "bounded mixed wait preserves item order and reports only readable socket");

    char buffer[32];
    if (mixed_ready > 0) {
        (void) tabos_socket_receive(receiver, buffer, sizeof(buffer));
    }
    mixed_items[0].returned_events = UINT32_MAX;
    mixed_items[1].returned_events = UINT32_MAX;
    tester_expect(context,
                  tabos_wait(mixed_items, 2U, 0U) == 0 && mixed_items[0].returned_events == 0U &&
                      mixed_items[1].returned_events == 0U,
                  "zero-time mixed wait clears old readiness without spurious wake");

    tabos_network_status_t initial_status;
    const bool status_available = tabos_network_get_status(&initial_status) == 0;
    if (status_available && initial_status.state == TABOS_NETWORK_ONLINE) {
        const int disconnect_status = run_fixture("--network-disconnect");
        const int lifecycle_ready   = tabos_wait(&device_item, 1U, 1000U);
        const bool mixed_payload_sent =
            sockets_ready && tabos_socket_send_to(sender, payload, sizeof(payload), &endpoint) == (int) sizeof(payload);
        bool mixed_device_observed = false;
        bool mixed_socket_observed = false;
        bool mixed_wait_valid      = mixed_payload_sent;
        const uint64_t deadline    = tabos_monotonic_ms() + 1000U;
        while (mixed_wait_valid && !mixed_socket_observed && tabos_monotonic_ms() < deadline) {
            mixed_items[0].returned_events = 0U;
            mixed_items[1].returned_events = 0U;
            const int ready                = tabos_wait(mixed_items, 2U, 0U);
            if (ready < 0) {
                mixed_wait_valid = false;
                break;
            }
            mixed_device_observed = mixed_device_observed || mixed_items[0].returned_events != 0U;
            mixed_socket_observed =
                mixed_socket_observed || (mixed_items[1].returned_events & TABOS_WAIT_READABLE) != 0U;
            if (!mixed_socket_observed) {
                (void) tabos_sleep_ms(1U);
            }
        }
        if (mixed_socket_observed) {
            (void) tabos_socket_receive(receiver, buffer, sizeof(buffer));
        }
        bool offline_event = false;
        tabos_device_event_t event;
        while (tabos_device_event_read(subscription, &event) == 0) {
            if (strcmp(event.device.name, TABOS_DEVICE_NAME_WIFI) == 0 && event.device.state == TABOS_DEVICE_OFFLINE) {
                offline_event = true;
            }
        }
        tester_expect(context,
                      disconnect_status == 81 && lifecycle_ready == 1 &&
                          (device_item.returned_events & (TABOS_WAIT_READABLE | TABOS_WAIT_STATE_CHANGED)) != 0U &&
                          offline_event,
                      "bounded device wait reports Wi-Fi disconnect lifecycle event");
        tester_expect(context, mixed_wait_valid && mixed_device_observed && mixed_socket_observed,
                      "device and socket readiness coexist in one ordered wait");
        if (initial_status.saved_config) {
            tester_expect(context, run_fixture("--network-connect-saved") == 83,
                          "saved Wi-Fi reconnect starts after lifecycle wait");
        }
    }

    if (sender >= 0) {
        (void) tabos_socket_close(sender);
    }
    if (receiver >= 0) {
        (void) tabos_socket_close(receiver);
    }
    if (subscription != TABOS_DEVICE_SUBSCRIPTION_INVALID) {
        (void) tabos_device_subscription_close(subscription);
    }
}

void tester_test_network(tester_context_t* context)
{
    test_tcp(context);
    test_udp(context);
    test_stale_handle(context);
    test_socket_capacity(context);
    test_full_table_accept(context);
    test_wait_sources(context);
}
