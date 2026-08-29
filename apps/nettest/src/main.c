#include <tabos/network.h>
#include <tabos/runtime_time.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    DEFAULT_TCP_PORT   = 39001,
    DEFAULT_UDP_PORT   = 39002,
    TCP_PAYLOAD_SIZE   = 1537,
    UDP_PAYLOAD_SIZE   = 257,
    RECEIVE_TIMEOUT_MS = 5000,
};

static const unsigned char TCP_HANDSHAKE[] = {'T', 'N', '0', '1'};

static void usage(FILE* stream)
{
    fprintf(stream, "Usage: nettest host [tcp-port [udp-port]]\n"
                    "       nettest --tcp host [port]\n"
                    "       nettest --udp host [port]\n"
                    "       nettest --listen tcp|udp [port]\n");
}

static bool parse_port(const char* text, uint16_t* port)
{
    char* end                 = NULL;
    errno                     = 0;
    const unsigned long value = strtoul(text, &end, 10);
    if (errno != 0 || text[0] == '\0' || end == NULL || *end != '\0' || value == 0U || value > UINT16_MAX) {
        return false;
    }
    *port = (uint16_t) value;
    return true;
}

static void fill_payload(unsigned char* payload, size_t size, unsigned int seed)
{
    for (size_t index = 0U; index < size; ++index) {
        payload[index] = (unsigned char) ((index * 37U + seed) & 0xffU);
    }
}

static int send_all(tabos_socket_t socket, const unsigned char* data, size_t size)
{
    size_t sent = 0U;
    while (sent < size) {
        size_t chunk = size - sent;
        if (chunk > TABOS_NETWORK_IO_MAX) {
            chunk = TABOS_NETWORK_IO_MAX;
        }
        const int result = tabos_socket_send(socket, data + sent, (uint32_t) chunk);
        if (result <= 0) {
            return -1;
        }
        sent += (size_t) result;
    }
    return 0;
}

static int receive_exact(tabos_socket_t socket, unsigned char* data, size_t size)
{
    size_t received         = 0U;
    const uint64_t deadline = tabos_monotonic_ms() + RECEIVE_TIMEOUT_MS;
    while (received < size) {
        size_t capacity = size - received;
        if (capacity > TABOS_NETWORK_IO_MAX) {
            capacity = TABOS_NETWORK_IO_MAX;
        }
        const int result = tabos_socket_receive(socket, data + received, (uint32_t) capacity);
        if (result < 0 && errno == EAGAIN && tabos_monotonic_ms() < deadline) {
            (void) tabos_sleep_ms(10U);
            continue;
        }
        if (result <= 0) {
            if (result < 0 && errno == EAGAIN) {
                errno = ETIMEDOUT;
            }
            return -1;
        }
        received += (size_t) result;
    }
    return 0;
}

static int run_tcp_client(const tabos_network_address_t* address, uint16_t port)
{
    unsigned char sent[TCP_PAYLOAD_SIZE];
    unsigned char received[TCP_PAYLOAD_SIZE];
    fill_payload(sent, sizeof(sent), 11U);
    memset(received, 0, sizeof(received));

    const tabos_socket_t socket = tabos_socket_open(address->family, TABOS_SOCKET_TCP);
    if (socket < 0) {
        fprintf(stderr, "nettest: TCP socket: %s\n", strerror(errno));
        return 1;
    }
    const tabos_socket_endpoint_t endpoint = {.address = *address, .port = port};
    int result                             = 1;
    if (tabos_socket_connect(socket, &endpoint) != 0) {
        fprintf(stderr, "nettest: TCP connect: %s\n", strerror(errno));
    } else if (tabos_socket_set_nonblocking(socket, true) != 0) {
        fprintf(stderr, "nettest: TCP nonblocking mode: %s (errno %d)\n", strerror(errno), errno);
    } else {
        unsigned char handshake[sizeof(TCP_HANDSHAKE)];
        if (receive_exact(socket, handshake, sizeof(handshake)) != 0) {
            fprintf(stderr, "nettest: TCP handshake receive: %s (errno %d)\n", strerror(errno), errno);
        } else if (memcmp(handshake, TCP_HANDSHAKE, sizeof(handshake)) != 0) {
            fprintf(stderr, "nettest: TCP handshake mismatch\n");
        } else if (tabos_socket_set_nonblocking(socket, false) != 0) {
            fprintf(stderr, "nettest: TCP blocking mode: %s (errno %d)\n", strerror(errno), errno);
        } else if (send_all(socket, sent, sizeof(sent)) != 0) {
            fprintf(stderr, "nettest: TCP send: %s\n", strerror(errno));
        } else if (tabos_socket_set_nonblocking(socket, true) != 0) {
            fprintf(stderr, "nettest: TCP nonblocking mode: %s (errno %d)\n", strerror(errno), errno);
        } else if (receive_exact(socket, received, sizeof(received)) != 0) {
            fprintf(stderr, "nettest: TCP receive: %s (errno %d)\n", strerror(errno), errno);
        } else if (memcmp(sent, received, sizeof(sent)) != 0) {
            fprintf(stderr, "nettest: TCP echo data mismatch\n");
        } else {
            printf("[PASS] TCP: %u bytes echoed by %s:%u\n", TCP_PAYLOAD_SIZE, address->text, port);
            result = 0;
        }
    }
    (void) tabos_socket_close(socket);
    return result;
}

static int run_udp_client(const tabos_network_address_t* address, uint16_t port)
{
    unsigned char sent[UDP_PAYLOAD_SIZE];
    unsigned char received[UDP_PAYLOAD_SIZE];
    fill_payload(sent, sizeof(sent), 29U);
    memset(received, 0, sizeof(received));

    const tabos_socket_t socket = tabos_socket_open(address->family, TABOS_SOCKET_UDP);
    if (socket < 0) {
        fprintf(stderr, "nettest: UDP socket: %s\n", strerror(errno));
        return 1;
    }
    const tabos_socket_endpoint_t endpoint = {.address = *address, .port = port};
    tabos_socket_endpoint_t peer;
    int result          = 1;
    const int sent_size = tabos_socket_send_to(socket, sent, sizeof(sent), &endpoint);
    if (sent_size != (int) sizeof(sent)) {
        fprintf(stderr, "nettest: UDP send: %s\n", strerror(errno));
    } else if (tabos_socket_set_nonblocking(socket, true) != 0) {
        fprintf(stderr, "nettest: UDP nonblocking mode: %s (errno %d)\n", strerror(errno), errno);
    } else {
        int received_size;
        const uint64_t deadline = tabos_monotonic_ms() + RECEIVE_TIMEOUT_MS;
        do {
            received_size = tabos_socket_receive_from(socket, received, sizeof(received), &peer);
            if (received_size < 0 && errno == EAGAIN && tabos_monotonic_ms() < deadline) {
                (void) tabos_sleep_ms(10U);
            } else {
                break;
            }
        } while (true);
        if (received_size < 0 && errno == EAGAIN) {
            errno = ETIMEDOUT;
        }
        if (received_size != (int) sizeof(received)) {
            fprintf(stderr, "nettest: UDP receive: %s (errno %d)\n", strerror(errno), errno);
        } else if (memcmp(sent, received, sizeof(sent)) != 0) {
            fprintf(stderr, "nettest: UDP echo data mismatch\n");
        } else if (peer.port != port || strcmp(peer.address.text, address->text) != 0) {
            fprintf(stderr, "nettest: UDP reply came from unexpected peer %s:%u\n", peer.address.text, peer.port);
        } else {
            printf("[PASS] UDP: %u-byte datagram echoed by %s:%u\n", UDP_PAYLOAD_SIZE, address->text, port);
            result = 0;
        }
    }
    (void) tabos_socket_close(socket);
    return result;
}

static tabos_socket_endpoint_t any_ipv4_endpoint(uint16_t port)
{
    tabos_socket_endpoint_t endpoint = {
        .address.family = TABOS_NETWORK_FAMILY_IPV4,
        .port           = port,
    };
    (void) strcpy(endpoint.address.text, "0.0.0.0");
    return endpoint;
}

static int run_tcp_server(uint16_t port)
{
    const tabos_socket_t server = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_TCP);
    if (server < 0) {
        fprintf(stderr, "nettest: TCP server socket: %s\n", strerror(errno));
        return 1;
    }
    const tabos_socket_endpoint_t endpoint = any_ipv4_endpoint(port);
    int result                             = 1;
    if (tabos_socket_bind(server, &endpoint) != 0 || tabos_socket_listen(server, 1U) != 0) {
        fprintf(stderr, "nettest: TCP listen: %s\n", strerror(errno));
    } else {
        printf("TCP echo listening on port %u\n", port);
        tabos_socket_endpoint_t peer;
        const tabos_socket_t client = tabos_socket_accept(server, &peer);
        if (client < 0) {
            fprintf(stderr, "nettest: TCP accept: %s\n", strerror(errno));
        } else {
            printf("TCP client %s:%u connected\n", peer.address.text, peer.port);
            unsigned char buffer[TABOS_NETWORK_IO_MAX];
            result = 0;
            for (;;) {
                const int size = tabos_socket_receive(client, buffer, sizeof(buffer));
                if (size == 0) {
                    break;
                }
                if (size < 0 || send_all(client, buffer, (size_t) size) != 0) {
                    fprintf(stderr, "nettest: TCP echo: %s\n", strerror(errno));
                    result = 1;
                    break;
                }
            }
            (void) tabos_socket_close(client);
        }
    }
    (void) tabos_socket_close(server);
    return result;
}

static int run_udp_server(uint16_t port)
{
    const tabos_socket_t socket = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_UDP);
    if (socket < 0) {
        fprintf(stderr, "nettest: UDP server socket: %s\n", strerror(errno));
        return 1;
    }
    const tabos_socket_endpoint_t endpoint = any_ipv4_endpoint(port);
    int result                             = 1;
    if (tabos_socket_bind(socket, &endpoint) != 0) {
        fprintf(stderr, "nettest: UDP bind: %s\n", strerror(errno));
    } else {
        printf("UDP echo listening on port %u\n", port);
        unsigned char buffer[TABOS_NETWORK_IO_MAX];
        tabos_socket_endpoint_t peer;
        const int size = tabos_socket_receive_from(socket, buffer, sizeof(buffer), &peer);
        if (size < 0) {
            fprintf(stderr, "nettest: UDP receive: %s\n", strerror(errno));
        } else if (tabos_socket_send_to(socket, buffer, (uint32_t) size, &peer) != size) {
            fprintf(stderr, "nettest: UDP echo: %s\n", strerror(errno));
        } else {
            printf("[PASS] UDP: echoed %d bytes to %s:%u\n", size, peer.address.text, peer.port);
            result = 0;
        }
    }
    (void) tabos_socket_close(socket);
    return result;
}

int main(int argc, char** argv)
{
    if (argc >= 3 && argc <= 4 && strcmp(argv[1], "--listen") == 0) {
        const bool tcp = strcmp(argv[2], "tcp") == 0;
        const bool udp = strcmp(argv[2], "udp") == 0;
        if (!tcp && !udp) {
            usage(stderr);
            return 2;
        }
        uint16_t port = tcp ? DEFAULT_TCP_PORT : DEFAULT_UDP_PORT;
        if (argc == 4 && !parse_port(argv[3], &port)) {
            fprintf(stderr, "nettest: invalid port\n");
            return 2;
        }
        return tcp ? run_tcp_server(port) : run_udp_server(port);
    }
    if (argc >= 2 && (strcmp(argv[1], "--tcp") == 0 || strcmp(argv[1], "--udp") == 0)) {
        if (argc < 3 || argc > 4) {
            usage(stderr);
            return 2;
        }
        const bool tcp = strcmp(argv[1], "--tcp") == 0;
        uint16_t port  = tcp ? DEFAULT_TCP_PORT : DEFAULT_UDP_PORT;
        if (argc == 4 && !parse_port(argv[3], &port)) {
            fprintf(stderr, "nettest: invalid port\n");
            return 2;
        }
        tabos_network_address_t address;
        if (tabos_network_resolve(argv[2], TABOS_NETWORK_FAMILY_IPV4, &address) != 0) {
            fprintf(stderr, "nettest: resolve %s: %s\n", argv[2], strerror(errno));
            return 1;
        }
        printf("Testing %s host %s (%s)\n", tcp ? "TCP" : "UDP", argv[2], address.text);
        return tcp ? run_tcp_client(&address, port) : run_udp_client(&address, port);
    }
    if (argc < 2 || argc > 4) {
        usage(stderr);
        return 2;
    }
    uint16_t tcp_port = DEFAULT_TCP_PORT;
    uint16_t udp_port = DEFAULT_UDP_PORT;
    if ((argc >= 3 && !parse_port(argv[2], &tcp_port)) || (argc == 4 && !parse_port(argv[3], &udp_port))) {
        fprintf(stderr, "nettest: invalid port\n");
        return 2;
    }
    tabos_network_address_t address;
    if (tabos_network_resolve(argv[1], TABOS_NETWORK_FAMILY_IPV4, &address) != 0) {
        fprintf(stderr, "nettest: resolve %s: %s\n", argv[1], strerror(errno));
        return 1;
    }
    printf("Testing host %s (%s)\n", argv[1], address.text);
    const int tcp_result = run_tcp_client(&address, tcp_port);
    if (tcp_result != 0) {
        fprintf(stderr, "nettest: TCP failed; UDP skipped (run nettest --udp %s to test it separately)\n", argv[1]);
        return 1;
    }
    const int udp_result = run_udp_client(&address, udp_port);
    return udp_result == 0 ? 0 : 1;
}
