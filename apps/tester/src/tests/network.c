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
    tester_expect(context,
                  replacement >= 0 && replacement != stale && stale_result < 0 && errno == EBADF,
                  "stale socket handle cannot close replacement");
    if (replacement >= 0) {
        tester_expect(context, tabos_socket_close(replacement) == 0, "replacement socket closes");
    }
}

void tester_test_network(tester_context_t* context)
{
    test_tcp(context);
    test_udp(context);
    test_stale_handle(context);
}
