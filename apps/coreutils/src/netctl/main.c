#include <tabos/network.h>
#include <tabos/ansi.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static void usage(FILE* stream)
{
    fprintf(stream,
            "Usage: netctl status\n       netctl connect\n       netctl connect --prompt\n       netctl disconnect\n");
}

static int show_status(void)
{
    tabos_network_status_t status;
    if (tabos_network_get_status(&status) != 0) {
        fprintf(stderr, "netctl: cannot get status: %s\n", strerror(errno));
        return 1;
    }
    printf("State: %s\n", tabos_network_state_name(status.state));
    printf("Saved config: %s\n", status.saved_config ? "yes" : "no");
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

int main(int argc, char** argv)
{
    if (argc == 2 && strcmp(argv[1], "status") == 0) {
        return show_status();
    }
    if (argc == 2 && strcmp(argv[1], "connect") == 0) {
        if (tabos_network_connect_saved() != 0) {
            fprintf(stderr, "netctl: cannot connect: %s\n", strerror(errno));
            return 1;
        }
        puts("Connection started.");
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "connect") == 0 && strcmp(argv[2], "--prompt") == 0) {
        fprintf(stderr, "netctl: interactive connection prompt is not implemented yet\n");
        return 1;
    }
    if (argc == 2 && strcmp(argv[1], "disconnect") == 0) {
        if (tabos_network_disconnect() != 0) {
            fprintf(stderr, "netctl: cannot disconnect: %s\n", strerror(errno));
            return 1;
        }
        puts("Disconnected.");
        return 0;
    }
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        usage(stdout);
        return 0;
    }
    usage(stderr);
    return 1;
}
