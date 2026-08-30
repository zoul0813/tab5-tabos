#include <tester/test.h>

#include <tabos/network.h>

#include <errno.h>
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
    tester_expect(context,
                  tabos_socket_send(client, message, sizeof(message)) == (int) sizeof(message) &&
                      tabos_socket_receive(accepted, buffer, sizeof(buffer)) == (int) sizeof(message) &&
                      memcmp(buffer, message, sizeof(message)) == 0,
                  "TCP transfers bytes");
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
    const tabos_socket_t stale = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_UDP);
    tester_expect(context, stale >= 0 && tabos_socket_close(stale) == 0, "socket closes before slot reuse");

    const tabos_socket_t replacement = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_UDP);
    errno                            = 0;
    const int stale_result           = tabos_socket_close(stale);
    tester_expect(context, replacement >= 0 && replacement != stale && stale_result < 0 && errno == EBADF,
                  "stale socket handle cannot close replacement");
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

void tester_test_network(tester_context_t* context)
{
    test_tcp(context);
    test_udp(context);
    test_stale_handle(context);
    test_socket_capacity(context);
    test_full_table_accept(context);
}
