#include <tabos/ansi.h>
#include <tabos/network.h>
#include <tabos/runtime_time.h>

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    CHANNEL_MAX        = 14,
    /* 20 MHz 2.4 GHz channels overlap unless five or more channels apart. */
    CHANNEL_OVERLAP    = 4,
    DEFAULT_INTERVAL_S = 5,
    MAX_REPEAT         = 1000,
    MAX_INTERVAL_S     = 3600,
    SIGNAL_BAR_WIDTH   = 4,
};

typedef struct {
        unsigned long channel;
        const char* ssid_filter;
        unsigned long repeat;
        unsigned long interval_s;
        bool survey;
} options_t;

static tabos_wifi_access_point_t access_points[TABOS_WIFI_SCAN_MAX];

static void usage(FILE* stream)
{
    fprintf(stream, "Usage: wifiscan [-c channel] [-s text] [-r count] [-i seconds] [--channels]\n"
                    "  -c channel   show only access points on one channel (1-14)\n"
                    "  -s text      show only SSIDs containing text, ignoring case\n"
                    "  -r count     repeat the scan count times (default 1)\n"
                    "  -i seconds   delay between repeated scans (default 5)\n"
                    "  --channels   summarize congestion per channel instead of listing\n");
}

static int parse_number(const char* text, unsigned long maximum, unsigned long* value)
{
    char* end = NULL;
    errno     = 0;
    const unsigned long parsed = strtoul(text, &end, 10);
    if (errno != 0 || text[0] == '\0' || end == NULL || *end != '\0' || parsed == 0U || parsed > maximum) {
        return -1;
    }
    *value = parsed;
    return 0;
}

static bool contains_ignoring_case(const char* text, const char* pattern)
{
    const size_t pattern_length = strlen(pattern);
    if (pattern_length == 0U) {
        return true;
    }
    for (const char* start = text; *start != '\0'; ++start) {
        size_t matched = 0U;
        while (matched < pattern_length && start[matched] != '\0' &&
               tolower((unsigned char) start[matched]) == tolower((unsigned char) pattern[matched])) {
            ++matched;
        }
        if (matched == pattern_length) {
            return true;
        }
    }
    return false;
}

static bool selected(const options_t* options, const tabos_wifi_access_point_t* access_point)
{
    if (options->channel != 0U && access_point->channel != options->channel) {
        return false;
    }
    if (options->ssid_filter != NULL && !contains_ignoring_case(access_point->ssid, options->ssid_filter)) {
        return false;
    }
    return true;
}

static unsigned int signal_level(int32_t signal_dbm)
{
    if (signal_dbm >= -55) {
        return 4U;
    }
    if (signal_dbm >= -67) {
        return 3U;
    }
    if (signal_dbm >= -75) {
        return 2U;
    }
    if (signal_dbm >= -85) {
        return 1U;
    }
    return 0U;
}

static const char* signal_color(unsigned int level)
{
    if (level >= 3U) {
        return ANSI_GREEN;
    }
    if (level == 2U) {
        return ANSI_YELLOW;
    }
    return ANSI_RED;
}

static void print_signal_bar(int32_t signal_dbm)
{
    const unsigned int level           = signal_level(signal_dbm);
    char bar[SIGNAL_BAR_WIDTH + 1]     = {0};
    for (unsigned int index = 0U; index < SIGNAL_BAR_WIDTH; ++index) {
        if (index < level) {
            bar[index] = '#';
        } else {
            bar[index] = '.';
        }
    }
    printf("%s%s" ANSI_RESET, signal_color(level), bar);
}

static void format_phy(uint32_t phy, char* text, size_t capacity)
{
    static const struct {
            uint32_t flag;
            const char* name;
    } names[] = {
        {TABOS_WIFI_PHY_11B,  "b" },
        {TABOS_WIFI_PHY_11G,  "g" },
        {TABOS_WIFI_PHY_11N,  "n" },
        {TABOS_WIFI_PHY_11AX, "ax"},
    };
    size_t length = 0U;
    text[0]       = '\0';
    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        const size_t name_length = strlen(names[index].name);
        if ((phy & names[index].flag) != 0U && length + name_length < capacity) {
            memcpy(text + length, names[index].name, name_length + 1U);
            length += name_length;
        }
    }
    if (length == 0U) {
        (void) snprintf(text, capacity, "-");
    }
}

static void print_list(const options_t* options, int count)
{
    printf("%4s %2s %-4s %-10s %-5s %-17s %s\n", "dBm", "CH", "SIG", "SECURITY", "PHY", "BSSID", "SSID");
    unsigned int shown = 0U;
    for (int index = 0; index < count; ++index) {
        const tabos_wifi_access_point_t* access_point = &access_points[index];
        if (!selected(options, access_point)) {
            continue;
        }
        char phy[8];
        format_phy(access_point->phy, phy, sizeof(phy));
        printf("%4ld %2lu ", (long) access_point->signal_dbm, (unsigned long) access_point->channel);
        print_signal_bar(access_point->signal_dbm);
        printf(" %-10s %-5s %02x:%02x:%02x:%02x:%02x:%02x ", tabos_wifi_security_name(access_point->security), phy,
               access_point->bssid[0], access_point->bssid[1], access_point->bssid[2], access_point->bssid[3],
               access_point->bssid[4], access_point->bssid[5]);
        if (access_point->ssid[0] == '\0') {
            printf(ANSI_CYAN "<hidden>" ANSI_RESET "\n");
        } else {
            printf("%s\n", access_point->ssid);
        }
        ++shown;
    }
    printf("%u of %d access point%s shown\n", shown, count, count == 1 ? "" : "s");
}

static unsigned int overlapping(const unsigned int* per_channel, unsigned int channel)
{
    unsigned int total = 0U;
    for (unsigned int other = 1U; other <= CHANNEL_MAX; ++other) {
        const unsigned int distance = other > channel ? other - channel : channel - other;
        if (distance <= CHANNEL_OVERLAP) {
            total += per_channel[other];
        }
    }
    return total;
}

static void print_survey(const options_t* options, int count)
{
    unsigned int per_channel[CHANNEL_MAX + 1] = {0};
    int32_t strongest[CHANNEL_MAX + 1]        = {0};
    for (int index = 0; index < count; ++index) {
        const tabos_wifi_access_point_t* access_point = &access_points[index];
        const uint32_t channel                        = access_point->channel;
        if (!selected(options, access_point) || channel == 0U || channel > CHANNEL_MAX) {
            continue;
        }
        if (per_channel[channel] == 0U || access_point->signal_dbm > strongest[channel]) {
            strongest[channel] = access_point->signal_dbm;
        }
        ++per_channel[channel];
    }

    printf("%2s %4s %9s %7s\n", "CH", "APs", "STRONGEST", "OVERLAP");
    for (unsigned int channel = 1U; channel <= CHANNEL_MAX; ++channel) {
        if (channel == CHANNEL_MAX && per_channel[channel] == 0U) {
            continue;
        }
        const unsigned int overlap = overlapping(per_channel, channel);
        if (per_channel[channel] == 0U) {
            printf("%2u %4u %9s %7u\n", channel, 0U, "-", overlap);
        } else {
            printf("%2u %4u %5ld dBm %7u\n", channel, per_channel[channel], (long) strongest[channel], overlap);
        }
    }

    static const unsigned int candidates[] = {1U, 6U, 11U};
    unsigned int best                      = candidates[0];
    unsigned int best_overlap              = overlapping(per_channel, best);
    for (size_t index = 1U; index < sizeof(candidates) / sizeof(candidates[0]); ++index) {
        const unsigned int overlap = overlapping(per_channel, candidates[index]);
        if (overlap < best_overlap) {
            best         = candidates[index];
            best_overlap = overlap;
        }
    }
    printf("Least congested of channels 1, 6, 11: " ANSI_GREEN "%u" ANSI_RESET " (%u overlapping access point%s)\n",
           best, best_overlap, best_overlap == 1U ? "" : "s");
}

static void report_error(int error)
{
    if (error == EBUSY) {
        fprintf(stderr, "wifiscan: radio busy (starting, connecting, or another scan); try again\n");
    } else if (error == ENETDOWN) {
        fprintf(stderr, "wifiscan: Wi-Fi radio is unavailable\n");
    } else {
        fprintf(stderr, "wifiscan: scan failed: %s\n", strerror(error));
    }
}

static int parse_options(int argc, char** argv, options_t* options)
{
    for (int index = 1; index < argc; ++index) {
        const char* argument = argv[index];
        if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
            usage(stdout);
            return 1;
        }
        if (strcmp(argument, "--channels") == 0) {
            options->survey = true;
        } else if (strcmp(argument, "-s") == 0 && index + 1 < argc) {
            options->ssid_filter = argv[++index];
        } else if (strcmp(argument, "-c") == 0 && index + 1 < argc) {
            if (parse_number(argv[++index], CHANNEL_MAX, &options->channel) != 0) {
                fprintf(stderr, "wifiscan: invalid channel\n");
                return -1;
            }
        } else if (strcmp(argument, "-r") == 0 && index + 1 < argc) {
            if (parse_number(argv[++index], MAX_REPEAT, &options->repeat) != 0) {
                fprintf(stderr, "wifiscan: invalid repeat count\n");
                return -1;
            }
        } else if (strcmp(argument, "-i") == 0 && index + 1 < argc) {
            if (parse_number(argv[++index], MAX_INTERVAL_S, &options->interval_s) != 0) {
                fprintf(stderr, "wifiscan: invalid interval\n");
                return -1;
            }
        } else {
            usage(stderr);
            return -1;
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    options_t options = {
        .repeat     = 1U,
        .interval_s = DEFAULT_INTERVAL_S,
    };
    const int parsed = parse_options(argc, argv, &options);
    if (parsed > 0) {
        return 0;
    }
    if (parsed < 0) {
        return 2;
    }

    for (unsigned long pass = 0U; pass < options.repeat; ++pass) {
        if (pass > 0U) {
            if (tabos_sleep_ms((uint32_t) (options.interval_s * 1000U)) != 0) {
                fprintf(stderr, "wifiscan: interrupted\n");
                return 1;
            }
            putchar('\n');
        }
        if (options.repeat > 1U) {
            printf("Scan %lu of %lu\n", pass + 1U, options.repeat);
        }
        const uint64_t started = tabos_monotonic_ms();
        const int count        = tabos_network_scan(access_points, TABOS_WIFI_SCAN_MAX);
        if (count < 0) {
            report_error(errno);
            return 1;
        }
        const uint64_t elapsed_ms = tabos_monotonic_ms() - started;
        if (options.survey) {
            print_survey(&options, count);
        } else {
            print_list(&options, count);
        }
        printf("Scan took %lu ms\n", (unsigned long) elapsed_ms);
    }
    return 0;
}
