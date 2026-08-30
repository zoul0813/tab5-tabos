#include <tabos/network.h>
#include <tabos/runtime_time.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    DEFAULT_COUNT = 4,
    DEFAULT_TIMEOUT_MS = 1000,
    DEFAULT_PAYLOAD_BYTES = 56,
    MAX_COUNT = 100,
};

static void usage(FILE* stream)
{
    fprintf(stream, "Usage: ping [-4|-6] [-c count] [-W timeout-ms] host\n");
}

static int parse_number(const char* text, unsigned long maximum, unsigned long* value)
{
    char* end = NULL;
    errno = 0;
    const unsigned long parsed = strtoul(text, &end, 10);
    if (errno != 0 || text[0] == '\0' || end == NULL || *end != '\0' || parsed == 0U || parsed > maximum) {
        return -1;
    }
    *value = parsed;
    return 0;
}

int main(int argc, char** argv)
{
    tabos_network_family_t family = TABOS_NETWORK_FAMILY_ANY;
    unsigned long count = DEFAULT_COUNT;
    unsigned long timeout_ms = DEFAULT_TIMEOUT_MS;
    const char* host = NULL;
    for (int index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "-4") == 0) {
            if (family == TABOS_NETWORK_FAMILY_IPV6) {
                usage(stderr);
                return 2;
            }
            family = TABOS_NETWORK_FAMILY_IPV4;
        } else if (strcmp(argv[index], "-6") == 0) {
            if (family == TABOS_NETWORK_FAMILY_IPV4) {
                usage(stderr);
                return 2;
            }
            family = TABOS_NETWORK_FAMILY_IPV6;
        } else if ((strcmp(argv[index], "-c") == 0 || strcmp(argv[index], "-W") == 0) && index + 1 < argc) {
            const bool is_count = strcmp(argv[index], "-c") == 0;
            unsigned long parsed;
            if (parse_number(argv[++index], is_count ? MAX_COUNT : 60000U, &parsed) != 0) {
                fprintf(stderr, "ping: invalid %s\n", is_count ? "count" : "timeout");
                return 2;
            }
            if (is_count) {
                count = parsed;
            } else {
                timeout_ms = parsed;
            }
        } else if (argv[index][0] == '-' || host != NULL) {
            usage(stderr);
            return 2;
        } else {
            host = argv[index];
        }
    }
    if (host == NULL) {
        usage(stderr);
        return 2;
    }

    tabos_network_address_t address;
    if (tabos_network_resolve(host, family, &address) != 0) {
        fprintf(stderr, "ping: %s: %s\n", host, strerror(errno));
        return 1;
    }
    printf("PING %s (%s): %u data bytes\n", host, address.text, DEFAULT_PAYLOAD_BYTES);
    unsigned long received = 0U;
    unsigned long transmitted = 0U;
    uint32_t minimum = UINT32_MAX;
    uint32_t maximum = 0U;
    uint64_t total = 0U;
    bool local_failure = false;
    for (unsigned long attempt = 0U; attempt < count; ++attempt) {
        tabos_network_echo_result_t echo;
        ++transmitted;
        if (tabos_network_echo(&address, (uint16_t) (attempt + 1U), DEFAULT_PAYLOAD_BYTES, (uint32_t) timeout_ms,
                               &echo) == 0) {
            printf("%lu bytes from %s: icmp_seq=%lu time=%lu ms\n", (unsigned long) echo.bytes, address.text,
                   (unsigned long) echo.sequence, (unsigned long) echo.round_trip_ms);
            ++received;
            total += echo.round_trip_ms;
            if (echo.round_trip_ms < minimum) {
                minimum = echo.round_trip_ms;
            }
            if (echo.round_trip_ms > maximum) {
                maximum = echo.round_trip_ms;
            }
        } else if (errno == ETIMEDOUT) {
            printf("Request timeout for icmp_seq %lu\n", attempt + 1U);
        } else {
            fprintf(stderr, "ping: echo failed: %s\n", strerror(errno));
            local_failure = true;
            break;
        }
        if (attempt + 1U < count) {
            (void) tabos_sleep_ms(1000U);
        }
    }
    const unsigned long lost_percent = (transmitted - received) * 100U / transmitted;
    printf("\n--- %s ping statistics ---\n", host);
    printf("%lu packets transmitted, %lu packets received, %lu%% packet loss\n", transmitted, received, lost_percent);
    if (received != 0U) {
        printf("round-trip min/avg/max = %lu/%lu/%lu ms\n", (unsigned long) minimum,
               (unsigned long) (total / received), (unsigned long) maximum);
    }
    return received != 0U && !local_failure ? 0 : 1;
}
