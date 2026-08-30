#include <tabos/network.h>
#include <tabos/filesystem.h>
#include <tabos/platform/platform.h>

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static int failures;

typedef struct {
        int socket;
        atomic_bool started;
        int result;
} blocking_receive_t;

static void expect(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static uint16_t reserve_port(int family, int type)
{
    const int descriptor = socket(family, type, 0);
    if (descriptor < 0) {
        return 0U;
    }
    struct sockaddr_storage address;
    memset(&address, 0, sizeof(address));
    socklen_t size;
    if (family == AF_INET) {
        struct sockaddr_in* ipv4 = (struct sockaddr_in*) &address;
        ipv4->sin_family         = AF_INET;
        ipv4->sin_addr.s_addr    = htonl(INADDR_LOOPBACK);
        size                     = sizeof(*ipv4);
    } else {
        struct sockaddr_in6* ipv6 = (struct sockaddr_in6*) &address;
        ipv6->sin6_family         = AF_INET6;
        ipv6->sin6_addr           = in6addr_loopback;
        size                      = sizeof(*ipv6);
    }
    uint16_t port = 0U;
    if (bind(descriptor, (const struct sockaddr*) &address, size) == 0 &&
        getsockname(descriptor, (struct sockaddr*) &address, &size) == 0) {
        port = family == AF_INET ? ntohs(((struct sockaddr_in*) &address)->sin_port) :
                                   ntohs(((struct sockaddr_in6*) &address)->sin6_port);
    }
    (void) close(descriptor);
    return port;
}

static void native_address(int family, uint16_t port, struct sockaddr_storage* address, socklen_t* size)
{
    memset(address, 0, sizeof(*address));
    if (family == AF_INET) {
        struct sockaddr_in* ipv4 = (struct sockaddr_in*) address;
        ipv4->sin_family         = AF_INET;
        ipv4->sin_port           = htons(port);
        ipv4->sin_addr.s_addr    = htonl(INADDR_LOOPBACK);
        *size                    = sizeof(*ipv4);
    } else {
        struct sockaddr_in6* ipv6 = (struct sockaddr_in6*) address;
        ipv6->sin6_family         = AF_INET6;
        ipv6->sin6_port           = htons(port);
        ipv6->sin6_addr           = in6addr_loopback;
        *size                     = sizeof(*ipv6);
    }
}

static platform_network_address_t portable_address(int family)
{
    platform_network_address_t address = {.family = family == AF_INET ? 4U : 6U};
    (void) snprintf(address.text, sizeof(address.text), "%s", family == AF_INET ? "127.0.0.1" : "::1");
    return address;
}

static void* blocking_receive(void* argument)
{
    blocking_receive_t* receive = argument;
    char byte;
    atomic_store_explicit(&receive->started, true, memory_order_release);
    receive->result = platform_network_socket_receive(receive->socket, &byte, sizeof(byte));
    return NULL;
}

static void test_interrupted_receive(void)
{
    const uint16_t port                       = reserve_port(AF_INET, SOCK_STREAM);
    const platform_network_address_t loopback = portable_address(AF_INET);
    const int server                          = platform_network_socket_open(loopback.family, TABOS_SOCKET_TCP);
    expect(port != 0U && server >= 0 && platform_network_socket_bind(server, &loopback, port) == 0 &&
               platform_network_socket_listen(server, 1U) == 0,
           "prepare interrupted receive server");

    const int client = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_storage target;
    socklen_t target_size;
    native_address(AF_INET, port, &target, &target_size);
    expect(client >= 0 && connect(client, (const struct sockaddr*) &target, target_size) == 0,
           "connect interrupted receive client");
    platform_network_address_t peer;
    uint16_t peer_port = 0U;
    const int accepted = platform_network_socket_accept(server, &peer, &peer_port);
    expect(accepted >= 0, "accept interrupted receive client");

    blocking_receive_t receive = {.socket = accepted};
    pthread_t thread;
    const bool thread_created = pthread_create(&thread, NULL, blocking_receive, &receive) == 0;
    expect(thread_created, "start blocking socket operation");
    if (thread_created) {
        const struct timespec poll = {.tv_nsec = 1000000L};
        while (!atomic_load_explicit(&receive.started, memory_order_acquire)) {
            (void) nanosleep(&poll, NULL);
        }
        const struct timespec settle = {.tv_nsec = 10000000L};
        (void) nanosleep(&settle, NULL);
        platform_network_socket_interrupt(accepted);
        const bool suspended = platform_network_socket_operations_suspend();
        expect(suspended, "wait for interrupted socket operation");
        platform_network_socket_dispose(accepted);
        if (suspended) {
            platform_network_socket_operations_resume();
        }
        expect(pthread_join(thread, NULL) == 0 && receive.result <= 0, "interrupted receive returns");
    } else if (accepted >= 0) {
        (void) platform_network_socket_close(accepted);
    }
    if (server >= 0) {
        (void) platform_network_socket_close(server);
    }
    if (client >= 0) {
        (void) close(client);
    }
}

static void test_tcp(int family)
{
    const uint16_t port = reserve_port(family, SOCK_STREAM);
    expect(port != 0U, "reserve TCP loopback port");
    const platform_network_address_t loopback = portable_address(family);
    const int server                          = platform_network_socket_open(loopback.family, TABOS_SOCKET_TCP);
    expect(server >= 0, "open TCP server");
    expect(platform_network_socket_bind(server, &loopback, port) == 0, "bind TCP server");
    expect(platform_network_socket_listen(server, 1U) == 0, "listen TCP server");

    const int client = socket(family, SOCK_STREAM, 0);
    struct sockaddr_storage target;
    socklen_t target_size;
    native_address(family, port, &target, &target_size);
    expect(client >= 0 && connect(client, (const struct sockaddr*) &target, target_size) == 0,
           "connect native TCP client");
    platform_network_address_t peer;
    uint16_t peer_port = 0U;
    const int accepted = platform_network_socket_accept(server, &peer, &peer_port);
    expect(accepted >= 0 && peer.family == loopback.family && peer_port != 0U, "accept TCP client");
    char buffer[32] = {0};
    expect(platform_network_socket_set_nonblocking(accepted, true) == 0 &&
               platform_network_socket_receive(accepted, buffer, sizeof(buffer)) == -TABOS_EAGAIN &&
               platform_network_socket_set_nonblocking(accepted, false) == 0,
           "TCP nonblocking receive returns EAGAIN");

    static const char outbound[] = "from-tabos";
    expect(platform_network_socket_send(accepted, outbound, sizeof(outbound)) == (int) sizeof(outbound) &&
               recv(client, buffer, sizeof(buffer), 0) == (ssize_t) sizeof(outbound) &&
               memcmp(buffer, outbound, sizeof(outbound)) == 0,
           "TCP send");
    static const char inbound[] = "to-tabos";
    memset(buffer, 0, sizeof(buffer));
    expect(send(client, inbound, sizeof(inbound), 0) == (ssize_t) sizeof(inbound) &&
               platform_network_socket_receive(accepted, buffer, sizeof(buffer)) == (int) sizeof(inbound) &&
               memcmp(buffer, inbound, sizeof(inbound)) == 0,
           "TCP receive");
    expect(platform_network_socket_close(accepted) == 0 && platform_network_socket_close(server) == 0,
           "close TCP sockets");
    (void) close(client);
}

static void test_udp(int family)
{
    const uint16_t port = reserve_port(family, SOCK_DGRAM);
    expect(port != 0U, "reserve UDP loopback port");
    const platform_network_address_t loopback = portable_address(family);
    const int server                          = platform_network_socket_open(loopback.family, TABOS_SOCKET_UDP);
    expect(server >= 0 && platform_network_socket_bind(server, &loopback, port) == 0, "bind UDP socket");

    const int client = socket(family, SOCK_DGRAM, 0);
    struct sockaddr_storage target;
    socklen_t target_size;
    native_address(family, port, &target, &target_size);
    static const char request[] = "udp-request";
    expect(sendto(client, request, sizeof(request), 0, (const struct sockaddr*) &target, target_size) ==
               (ssize_t) sizeof(request),
           "send native UDP datagram");
    char buffer[32] = {0};
    platform_network_address_t peer;
    uint16_t peer_port = 0U;
    expect(platform_network_socket_receive_from(server, buffer, sizeof(buffer), &peer, &peer_port) ==
                   (int) sizeof(request) &&
               memcmp(buffer, request, sizeof(request)) == 0 && peer_port != 0U,
           "receive UDP datagram");
    static const char response[] = "udp-response";
    expect(platform_network_socket_send_to(server, response, sizeof(response), &peer, peer_port) ==
                   (int) sizeof(response) &&
               recv(client, buffer, sizeof(buffer), 0) == (ssize_t) sizeof(response) &&
               memcmp(buffer, response, sizeof(response)) == 0,
           "reply UDP datagram");
    expect(platform_network_socket_close(server) == 0, "close UDP socket");
    (void) close(client);
}

int main(void)
{
    test_tcp(AF_INET);
    test_udp(AF_INET);
    test_tcp(AF_INET6);
    test_udp(AF_INET6);
    test_interrupted_receive();
    return failures == 0 ? 0 : 1;
}
