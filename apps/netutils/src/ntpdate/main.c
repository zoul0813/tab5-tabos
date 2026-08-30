#include <tabos/clock.h>
#include <tabos/network.h>
#include <tabos/wait.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    NTP_PORT                 = 123U,
    NTP_PACKET_BYTES         = 48U,
    NTP_TRANSMIT_SECONDS     = 40U,
    NTP_UNIX_EPOCH_OFFSET    = 2208988800U,
    NTP_RESPONSE_TIMEOUT_MS  = 5000U,
    NTP_MODE_SERVER          = 4U,
    NTP_STRATUM_UNSYNCHRONIZED = 0U,
    NTP_STRATUM_INVALID      = 16U,
};

static void usage(FILE* stream)
{
    fprintf(stream, "Usage: ntpdate [-4|-6] [server]\n");
}

static uint32_t read_u32_be(const uint8_t* bytes)
{
    return (uint32_t) bytes[0] << 24U | (uint32_t) bytes[1] << 16U | (uint32_t) bytes[2] << 8U | bytes[3];
}

static bool valid_response(const uint8_t* packet, int size)
{
    if (packet == NULL || size < NTP_PACKET_BYTES) {
        return false;
    }
    const uint8_t leap_indicator = packet[0] >> 6U;
    const uint8_t mode           = packet[0] & 0x07U;
    const uint8_t stratum        = packet[1];
    if (leap_indicator == 3U || mode != NTP_MODE_SERVER || stratum == NTP_STRATUM_UNSYNCHRONIZED ||
        stratum >= NTP_STRATUM_INVALID) {
        return false;
    }
    return read_u32_be(packet + NTP_TRANSMIT_SECONDS) >= NTP_UNIX_EPOCH_OFFSET;
}

int main(int argc, char** argv)
{
    tabos_network_family_t family = TABOS_NETWORK_FAMILY_IPV4;
    const char* server            = NULL;
    for (int index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "-4") == 0) {
            if (family == TABOS_NETWORK_FAMILY_IPV6) {
                usage(stderr);
                return 2;
            }
            family = TABOS_NETWORK_FAMILY_IPV4;
        } else if (strcmp(argv[index], "-6") == 0) {
            if (family == TABOS_NETWORK_FAMILY_IPV4 && index > 1) {
                usage(stderr);
                return 2;
            }
            family = TABOS_NETWORK_FAMILY_IPV6;
        } else if (strcmp(argv[index], "-h") == 0 || strcmp(argv[index], "--help") == 0) {
            if (argc != 2) {
                usage(stderr);
                return 2;
            }
            usage(stdout);
            return 0;
        } else if (argv[index][0] == '-' || server != NULL) {
            usage(stderr);
            return 2;
        } else {
            server = argv[index];
        }
    }
    if (server == NULL) {
        server = "pool.ntp.org";
    }
    tabos_network_address_t address;
    if (tabos_network_resolve(server, family, &address) != 0) {
        fprintf(stderr, "ntpdate: resolve %s: %s\n", server, strerror(errno));
        return 1;
    }

    const tabos_socket_t socket = tabos_socket_open(address.family, TABOS_SOCKET_UDP);
    if (socket < 0) {
        fprintf(stderr, "ntpdate: socket: %s\n", strerror(errno));
        return 1;
    }
    const tabos_socket_endpoint_t endpoint = {.address = address, .port = NTP_PORT};
    uint8_t request[NTP_PACKET_BYTES] = {0};
    request[0]                        = 0x23U;
    uint8_t response[NTP_PACKET_BYTES];
    tabos_socket_endpoint_t peer;
    int result = 1;
    if (tabos_socket_send_to(socket, request, sizeof(request), &endpoint) != (int) sizeof(request)) {
        fprintf(stderr, "ntpdate: send: %s\n", strerror(errno));
    } else {
        tabos_wait_item_t wait_item = {.socket = socket, .events = TABOS_WAIT_READABLE};
        const int ready             = tabos_wait_set(&wait_item, 1U, NTP_RESPONSE_TIMEOUT_MS);
        if (ready == 0) {
            fprintf(stderr, "ntpdate: timed out waiting for %s\n", server);
        } else if (ready < 0) {
            fprintf(stderr, "ntpdate: wait: %s\n", strerror(errno));
        } else if ((wait_item.returned_events & TABOS_WAIT_READABLE) == 0U) {
            fprintf(stderr, "ntpdate: socket became unavailable\n");
        } else {
            const int received = tabos_socket_receive_from(socket, response, sizeof(response), &peer);
            if (received < 0) {
                fprintf(stderr, "ntpdate: receive: %s\n", strerror(errno));
            } else if (peer.port != NTP_PORT || peer.address.family != address.family ||
                       strcmp(peer.address.text, address.text) != 0 || !valid_response(response, received)) {
                fprintf(stderr, "ntpdate: invalid response from %s\n", server);
                errno = EINVAL;
            } else {
                const int64_t seconds = (int64_t) read_u32_be(response + NTP_TRANSMIT_SECONDS) -
                                        (int64_t) NTP_UNIX_EPOCH_OFFSET;
                if (tabos_clock_set_epoch(seconds) != 0) {
                    fprintf(stderr, "ntpdate: set clock: %s\n", strerror(errno));
                } else {
                    printf("ntpdate: set UTC clock to %lld from %s (%s)\n", (long long) seconds, server,
                           address.text);
                    result = 0;
                }
            }
        }
    }
    (void) tabos_socket_close(socket);
    return result;
}
