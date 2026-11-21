#include <tabos/network.h>
#include <tabos/ansi.h>
#include <tabos/runtime_time.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
    /* The kernel allows three 20-second attempts with 1-second retry delays. */
    CONNECT_TIMEOUT_MS = 75000,
    CONNECT_POLL_MS    = 500,
    SCAN_BUSY_RETRIES  = 3,
    SCAN_BUSY_DELAY_MS = 2000,
    CHOICE_MAX         = 40,
};

typedef struct {
        const char* ssid;
        bool prompt;
        bool save;
} connect_options_t;

static tabos_wifi_access_point_t access_points[TABOS_WIFI_SCAN_MAX];

static void usage(FILE* stream)
{
    fprintf(stream, "Usage: netctl status\n"
                    "       netctl scan\n"
                    "       netctl connect                    join the saved network\n"
                    "       netctl connect --prompt [--save]  choose a scanned network\n"
                    "       netctl connect --ssid NAME [--save]\n"
                    "       netctl disconnect\n"
                    "       netctl save                       save the current connection\n"
                    "       netctl forget                     remove the saved network\n"
                    "       netctl autoconnect on|off\n");
}

static void report_busy_or_error(const char* action, int error)
{
    if (error == EBUSY) {
        fprintf(stderr, "netctl: cannot %s: radio busy (starting, connecting, or another scan); try again\n", action);
    } else {
        fprintf(stderr, "netctl: cannot %s: %s\n", action, strerror(error));
    }
}

static void report_profile_error(const char* action, int error)
{
    if (error == ENODEV) {
        fprintf(stderr, "netctl: cannot %s: T: storage is not mounted\n", action);
    } else if (error == EINVAL) {
        fprintf(stderr, "netctl: cannot %s: T:/etc/wifi.conf is damaged; fix or remove it\n", action);
    } else if (error == ENETDOWN) {
        fprintf(stderr, "netctl: cannot %s: not connected\n", action);
    } else {
        fprintf(stderr, "netctl: cannot %s: %s\n", action, strerror(error));
    }
}

static int show_status(void)
{
    tabos_network_status_t status;
    if (tabos_network_get_status(&status) != 0) {
        fprintf(stderr, "netctl: cannot get status: %s\n", strerror(errno));
        return 1;
    }
    printf("State: %s\n", tabos_network_state_name(status.state));
    if (status.saved_ssid[0] != '\0') {
        printf("Saved network: %s\n", status.saved_ssid);
    } else {
        printf("Saved network: none\n");
    }
    printf("Autoconnect: %s\n", status.auto_connect ? "on" : "off");
    printf("Attempts: %lu\n", (unsigned long) status.attempts);
    if (status.ssid[0] != '\0') {
        printf("SSID: %s\n", status.ssid);
    }
    if (status.state == TABOS_NETWORK_ONLINE) {
        printf("Signal: %ld dBm\n", (long) status.signal_dbm);
    }
    if (status.ipv4[0] != '\0') {
        printf("IPv4: " ANSI_GREEN "%s" ANSI_RESET "\n", status.ipv4);
    }
    printf("Hostname: " ANSI_GREEN "%s" ANSI_RESET "\n", status.hostname);
    if (status.last_failure[0] != '\0') {
        printf("Last failure: %s\n", status.last_failure);
    }
    return 0;
}

static int scan(void)
{
    for (unsigned int attempt = 0U;; ++attempt) {
        const int count = tabos_network_scan(access_points, TABOS_WIFI_SCAN_MAX);
        if (count >= 0 || errno != EBUSY || attempt + 1U >= SCAN_BUSY_RETRIES) {
            return count;
        }
        (void) tabos_sleep_ms(SCAN_BUSY_DELAY_MS);
    }
}

static int show_scan(void)
{
    const int count = tabos_network_scan(access_points, TABOS_WIFI_SCAN_MAX);
    if (count < 0) {
        report_busy_or_error("scan", errno);
        return 1;
    }
    printf("%-8s %-3s %-10s %s\n", "SIGNAL", "CH", "SECURITY", "SSID");
    for (int index = 0; index < count; ++index) {
        const tabos_wifi_access_point_t* access_point = &access_points[index];
        const char* ssid                              = access_point->ssid;
        if (ssid[0] == '\0') {
            ssid = "<hidden>";
        }
        printf("%4ld dBm %3lu %-10s %s\n", (long) access_point->signal_dbm, (unsigned long) access_point->channel,
               tabos_wifi_security_name(access_point->security), ssid);
    }
    printf("%d access point%s\n", count, count == 1 ? "" : "s");
    return 0;
}

/* Reads one line from the terminal. TabOS stdin never echoes, so visible input
 * is echoed here and hidden input is shown as '*'. Arrow-key sequences are
 * discarded. Returns false when input ends. */
static bool read_line(char* buffer, size_t capacity, bool hidden)
{
    size_t length         = 0U;
    unsigned int swallow  = 0U;
    buffer[0]             = '\0';
    for (;;) {
        char character = '\0';
        if (read(STDIN_FILENO, &character, 1U) != 1) {
            buffer[length] = '\0';
            return false;
        }
        if (swallow > 0U) {
            --swallow;
            continue;
        }
        if (character == '\x1b') {
            swallow = 2U;
            continue;
        }
        if (character == '\n' || character == '\r') {
            putchar('\n');
            buffer[length] = '\0';
            return true;
        }
        if (character == '\b' || character == 0x7f) {
            if (length > 0U) {
                --length;
                fputs("\b \b", stdout);
            }
            continue;
        }
        if ((unsigned char) character < 32U || length + 1U >= capacity) {
            continue;
        }
        buffer[length] = character;
        ++length;
        if (hidden) {
            putchar('*');
        } else {
            putchar(character);
        }
    }
}

static bool ask_yes(const char* question)
{
    char answer[8];
    printf("%s [y/N] ", question);
    if (!read_line(answer, sizeof(answer), false)) {
        return false;
    }
    return answer[0] == 'y' || answer[0] == 'Y';
}

/* Lists scanned networks once per SSID, strongest first; hidden networks are
 * omitted because they must be typed. Returns the number of choices. */
static int list_choices(int count, const tabos_wifi_access_point_t** choices)
{
    int listed = 0;
    for (int index = 0; index < count && listed < CHOICE_MAX; ++index) {
        const tabos_wifi_access_point_t* access_point = &access_points[index];
        if (access_point->ssid[0] == '\0') {
            continue;
        }
        bool duplicate = false;
        for (int seen = 0; seen < listed; ++seen) {
            if (strcmp(choices[seen]->ssid, access_point->ssid) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        choices[listed] = access_point;
        ++listed;
        printf("%2d) %4ld dBm  ch %-2lu %-10s %s\n", listed, (long) access_point->signal_dbm,
               (unsigned long) access_point->channel, tabos_wifi_security_name(access_point->security),
               access_point->ssid);
    }
    return listed;
}

static bool parse_choice(const char* text, int listed, int* choice)
{
    char* end           = NULL;
    const long selected = strtol(text, &end, 10);
    if (end == text || *end != '\0' || selected < 1 || selected > listed) {
        return false;
    }
    *choice = (int) selected;
    return true;
}

/* Chooses a network from a scan, or accepts a typed name for hidden networks.
 * Sets open to true only when the scan proved the network needs no password. */
static bool choose_network(char* ssid, size_t capacity, bool* open)
{
    *open           = false;
    const int count = scan();
    const tabos_wifi_access_point_t* choices[CHOICE_MAX];
    int listed = 0;
    if (count < 0) {
        report_busy_or_error("scan", errno);
        puts("Enter a network name instead.");
    } else {
        listed = list_choices(count, choices);
        if (listed == 0) {
            puts("No named networks found.");
        }
    }
    char answer[TABOS_NETWORK_SSID_MAX + 1];
    if (listed > 0) {
        printf("Network number, or name of a hidden network (Enter to cancel): ");
    } else {
        printf("Network name (Enter to cancel): ");
    }
    if (!read_line(answer, sizeof(answer), false) || answer[0] == '\0') {
        return false;
    }
    int choice = 0;
    if (parse_choice(answer, listed, &choice)) {
        const tabos_wifi_access_point_t* selected = choices[choice - 1];
        (void) snprintf(ssid, capacity, "%s", selected->ssid);
        *open = selected->security == TABOS_WIFI_SECURITY_OPEN;
        return true;
    }
    (void) snprintf(ssid, capacity, "%s", answer);
    return true;
}

/* Rejoins the saved network after a failed attempt, including a wrong password
 * for the saved SSID itself: the kernel now holds the rejected credentials and
 * only wifi.conf still has the working ones. */
static void restore_saved_network(void)
{
    tabos_network_status_t status;
    if (tabos_network_get_status(&status) != 0 || status.saved_ssid[0] == '\0') {
        return;
    }
    printf("Rejoining saved network %s.\n", status.saved_ssid);
    if (tabos_network_connect_saved() != 0) {
        fprintf(stderr, "netctl: cannot rejoin saved network: %s\n", strerror(errno));
    }
}

/* Waits for the started connection. The service retries three times; FAILED with
 * three attempts is final, while FAILED below three is a retry in progress. */
static bool wait_online(tabos_network_status_t* status)
{
    fputs("Connecting", stdout);
    for (uint32_t waited = 0U; waited < CONNECT_TIMEOUT_MS; waited += CONNECT_POLL_MS) {
        (void) tabos_sleep_ms(CONNECT_POLL_MS);
        if (tabos_network_get_status(status) != 0) {
            putchar('\n');
            return false;
        }
        if (status->state == TABOS_NETWORK_ONLINE && status->ipv4[0] != '\0') {
            putchar('\n');
            return true;
        }
        if (status->state == TABOS_NETWORK_FAILED && status->attempts >= 3U) {
            putchar('\n');
            return false;
        }
        if (waited % 2000U == 0U) {
            putchar('.');
        }
    }
    putchar('\n');
    (void) snprintf(status->last_failure, sizeof(status->last_failure), "timed out");
    return false;
}

static int connect_interactive(const connect_options_t* options)
{
    (void) setvbuf(stdout, NULL, _IONBF, 0);
    char ssid[TABOS_NETWORK_SSID_MAX + 1];
    bool open = false;
    if (options->ssid != NULL) {
        if (options->ssid[0] == '\0' || strlen(options->ssid) > TABOS_NETWORK_SSID_MAX) {
            fprintf(stderr, "netctl: network name must be 1 to %d bytes\n", TABOS_NETWORK_SSID_MAX);
            return 2;
        }
        (void) snprintf(ssid, sizeof(ssid), "%s", options->ssid);
    } else if (!choose_network(ssid, sizeof(ssid), &open)) {
        puts("Cancelled.");
        return 1;
    }

    char password[TABOS_NETWORK_PASSWORD_MAX + 1] = {0};
    if (!open) {
        printf("Password for %s (Enter for an open network): ", ssid);
        if (!read_line(password, sizeof(password), true)) {
            memset(password, 0, sizeof(password));
            puts("Cancelled.");
            return 1;
        }
    }
    const int started = tabos_network_connect(ssid, password);
    const int error   = errno;
    memset(password, 0, sizeof(password));
    if (started != 0) {
        fprintf(stderr, "netctl: cannot connect: %s\n", strerror(error));
        return 1;
    }

    tabos_network_status_t status;
    if (!wait_online(&status)) {
        fprintf(stderr, "netctl: could not join %s: %s\n", ssid,
                status.last_failure[0] != '\0' ? status.last_failure : "connection failed");
        puts("Saved network unchanged.");
        restore_saved_network();
        return 1;
    }
    printf("Connected to %s; IPv4 " ANSI_GREEN "%s" ANSI_RESET "\n", ssid, status.ipv4);
    /* Offer to save even for the saved SSID: this connection may have proved a
     * changed password. The kernel saves the credentials it is using. */
    const char* question = "Save as the default network?";
    if (strcmp(status.saved_ssid, ssid) == 0) {
        question = "Update the saved password for this network?";
    }
    if (options->save || ask_yes(question)) {
        if (tabos_network_save_profile() != 0) {
            report_profile_error("save network", errno);
            return 1;
        }
        printf("Saved %s to T:/etc/wifi.conf.\n", ssid);
    }
    return 0;
}

static int parse_connect(int argc, char** argv, connect_options_t* options)
{
    for (int index = 2; index < argc; ++index) {
        if (strcmp(argv[index], "--prompt") == 0) {
            options->prompt = true;
        } else if (strcmp(argv[index], "--save") == 0) {
            options->save = true;
        } else if (strcmp(argv[index], "--ssid") == 0 && index + 1 < argc) {
            options->ssid = argv[++index];
        } else {
            return -1;
        }
    }
    if (options->prompt && options->ssid != NULL) {
        return -1;
    }
    if (options->save && !options->prompt && options->ssid == NULL) {
        return -1;
    }
    return 0;
}

static int save(void)
{
    tabos_network_status_t status;
    if (tabos_network_save_profile() != 0) {
        report_profile_error("save network", errno);
        return 1;
    }
    if (tabos_network_get_status(&status) == 0) {
        printf("Saved %s to T:/etc/wifi.conf.\n", status.saved_ssid);
    }
    return 0;
}

static int forget(void)
{
    tabos_network_status_t status;
    const bool had_status = tabos_network_get_status(&status) == 0;
    if (tabos_network_forget_profile() != 0) {
        report_profile_error("forget network", errno);
        return 1;
    }
    if (had_status && status.saved_ssid[0] != '\0') {
        printf("Forgot %s. The current connection stays up until disconnect or restart.\n", status.saved_ssid);
    } else {
        puts("No saved network.");
    }
    return 0;
}

static int set_auto_connect(const char* value)
{
    bool enabled = false;
    if (strcmp(value, "on") == 0) {
        enabled = true;
    } else if (strcmp(value, "off") != 0) {
        usage(stderr);
        return 2;
    }
    if (tabos_network_set_auto_connect(enabled) != 0) {
        report_profile_error("change autoconnect", errno);
        return 1;
    }
    printf("Autoconnect %s.\n", enabled ? "on" : "off");
    return 0;
}

int main(int argc, char** argv)
{
    if (argc == 2 && strcmp(argv[1], "status") == 0) {
        return show_status();
    }
    if (argc == 2 && strcmp(argv[1], "scan") == 0) {
        return show_scan();
    }
    if (argc == 2 && strcmp(argv[1], "connect") == 0) {
        if (tabos_network_connect_saved() != 0) {
            if (errno == ENOENT) {
                fprintf(stderr, "netctl: no saved network; use netctl connect --prompt\n");
            } else {
                fprintf(stderr, "netctl: cannot connect: %s\n", strerror(errno));
            }
            return 1;
        }
        puts("Connection started.");
        return 0;
    }
    if (argc >= 3 && strcmp(argv[1], "connect") == 0) {
        connect_options_t options = {0};
        if (parse_connect(argc, argv, &options) != 0) {
            usage(stderr);
            return 2;
        }
        return connect_interactive(&options);
    }
    if (argc == 2 && strcmp(argv[1], "disconnect") == 0) {
        if (tabos_network_disconnect() != 0) {
            fprintf(stderr, "netctl: cannot disconnect: %s\n", strerror(errno));
            return 1;
        }
        puts("Disconnected.");
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "save") == 0) {
        return save();
    }
    if (argc == 2 && strcmp(argv[1], "forget") == 0) {
        return forget();
    }
    if (argc == 3 && strcmp(argv[1], "autoconnect") == 0) {
        return set_auto_connect(argv[2]);
    }
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        usage(stdout);
        return 0;
    }
    usage(stderr);
    return 1;
}
