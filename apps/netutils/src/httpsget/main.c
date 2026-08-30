#include <tabos/tls.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    HTTPS_PORT = 443U,
    HOST_MAX = 253,
    PATH_MAX = 512,
    BUFFER_SIZE = 1024,
};

static void usage(FILE* stream)
{
    fprintf(stream, "Usage: httpsget [-c count] https://host[/path]\n");
}

static bool parse_url(const char* url, char* host, size_t host_capacity, char* path, size_t path_capacity)
{
    static const char prefix[] = "https://";
    if (url == NULL || strncmp(url, prefix, sizeof(prefix) - 1U) != 0) {
        return false;
    }
    const char* authority = url + sizeof(prefix) - 1U;
    const char* separator = strchr(authority, '/');
    const size_t host_length = separator == NULL ? strlen(authority) : (size_t) (separator - authority);
    if (host_length == 0U || host_length >= host_capacity || strchr(authority, '@') != NULL || strchr(authority, ':') != NULL) {
        return false;
    }
    memcpy(host, authority, host_length);
    host[host_length] = '\0';
    if (separator == NULL) {
        (void) snprintf(path, path_capacity, "/");
    } else if (strlen(separator) >= path_capacity) {
        return false;
    } else {
        (void) snprintf(path, path_capacity, "%s", separator);
    }
    return true;
}

static int send_all(tabos_tls_t connection, const char* data, size_t size)
{
    size_t sent = 0U;
    while (sent < size) {
        const int result = tabos_tls_send(connection, data + sent, (uint32_t) (size - sent));
        if (result < 0) {
            return -1;
        }
        sent += (size_t) result;
    }
    return 0;
}

int main(int argc, char** argv)
{
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage(stdout);
        return 0;
    }
    unsigned int count = 1U;
    const char* url = NULL;
    for (int index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "-c") == 0 && index + 1 < argc) {
            char* end = NULL;
            const unsigned long parsed = strtoul(argv[++index], &end, 10);
            if (end == argv[index] || *end != '\0' || parsed == 0UL || parsed > 16UL) {
                usage(stderr);
                return 2;
            }
            count = (unsigned int) parsed;
        } else if (argv[index][0] == '-' || url != NULL) {
            usage(stderr);
            return 2;
        } else {
            url = argv[index];
        }
    }
    if (url == NULL) {
        usage(stderr);
        return 2;
    }
    char host[HOST_MAX + 1];
    char path[PATH_MAX + 1];
    if (!parse_url(url, host, sizeof(host), path, sizeof(path))) {
        fprintf(stderr, "httpsget: expected https://host[/path]; custom ports, user info, and IPv6 literals are unsupported\n");
        return 2;
    }
    char request[HOST_MAX + PATH_MAX + 128];
    const int request_size = snprintf(request, sizeof(request),
                                      "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: TabOS/0.0.1\r\nConnection: close\r\n\r\n",
                                      path, host);
    if (request_size < 0 || (size_t) request_size >= sizeof(request)) {
        fprintf(stderr, "httpsget: request too large\n");
        return 1;
    }
    for (unsigned int attempt = 0U; attempt < count; ++attempt) {
        const tabos_tls_t connection = tabos_tls_connect(host, HTTPS_PORT);
        if (connection < 0) {
            fprintf(stderr, "httpsget: connect %s: %s\n", host, strerror(errno));
            return 1;
        }
        int result = 1;
        if (send_all(connection, request, (size_t) request_size) != 0) {
            fprintf(stderr, "httpsget: send: %s\n", strerror(errno));
        } else {
        uint8_t buffer[BUFFER_SIZE];
        for (;;) {
            const int received = tabos_tls_receive(connection, buffer, sizeof(buffer));
            if (received < 0) {
                fprintf(stderr, "httpsget: receive: %s\n", strerror(errno));
                break;
            }
            if (received == 0) {
                result = 0;
                break;
            }
            (void) fwrite(buffer, 1U, (size_t) received, stdout);
        }
        }
        (void) tabos_tls_close(connection);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}
