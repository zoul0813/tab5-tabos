#include <tabos/tls.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    HOST_MAX = 253,
    PATH_MAX = 512,
    BUFFER_SIZE = 1024,
};

static void usage(FILE* stream)
{
    fprintf(stream, "Usage: fetch https://host[/path] [destination]\n");
}

static void default_destination(const char* path, char* destination, size_t capacity)
{
    const char* name = strrchr(path, '/');
    name = name == NULL ? path : name + 1;
    const char* query = strchr(name, '?');
    const size_t length = query == NULL ? strlen(name) : (size_t) (query - name);
    if (length == 0U || (length == 1U && name[0] == '.') ||
        (length == 2U && name[0] == '.' && name[1] == '.')) {
        (void) snprintf(destination, capacity, "download");
        return;
    }
    if (length >= capacity) {
        (void) snprintf(destination, capacity, "download");
        return;
    }
    memcpy(destination, name, length);
    destination[length] = '\0';
}

static bool parse_url(const char* url, char* host, char* path)
{
    static const char prefix[] = "https://";
    if (url == NULL || strncmp(url, prefix, sizeof(prefix) - 1U) != 0) {
        return false;
    }
    const char* authority = url + sizeof(prefix) - 1U;
    const char* separator = strchr(authority, '/');
    const size_t length = separator == NULL ? strlen(authority) : (size_t) (separator - authority);
    if (length == 0U || length > HOST_MAX || strchr(authority, ':') != NULL || strchr(authority, '@') != NULL) {
        return false;
    }
    memcpy(host, authority, length);
    host[length] = '\0';
    if (separator == NULL) {
        (void) snprintf(path, PATH_MAX + 1U, "/");
    } else if (strlen(separator) > PATH_MAX) {
        return false;
    } else {
        (void) snprintf(path, PATH_MAX + 1U, "%s", separator);
    }
    return true;
}

static int send_all(tabos_tls_t connection, const char* data, size_t length)
{
    size_t offset = 0U;
    while (offset < length) {
        const int sent = tabos_tls_send(connection, data + offset, (uint32_t) (length - offset));
        if (sent < 0) {
            return -1;
        }
        offset += (size_t) sent;
    }
    return 0;
}

static size_t body_offset(const uint8_t* buffer, size_t size)
{
    for (size_t index = 3U; index < size; ++index) {
        if (buffer[index - 3U] == '\r' && buffer[index - 2U] == '\n' && buffer[index - 1U] == '\r' &&
            buffer[index] == '\n') {
            return index + 1U;
        }
    }
    return 0U;
}

int main(int argc, char** argv)
{
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage(stdout);
        return 0;
    }
    if (argc != 2 && argc != 3) {
        usage(stderr);
        return 2;
    }
    char host[HOST_MAX + 1];
    char path[PATH_MAX + 1];
    if (!parse_url(argv[1], host, path)) {
        fprintf(stderr, "fetch: only https://host[/path] URLs are supported\n");
        return 2;
    }
    char destination[256];
    if (argc == 3) {
        (void) snprintf(destination, sizeof(destination), "%s", argv[2]);
    } else {
        default_destination(path, destination, sizeof(destination));
    }
    char temporary[256];
    if (snprintf(temporary, sizeof(temporary), "%s.part", destination) < 0 || strlen(destination) + 5U >= sizeof(temporary)) {
        fprintf(stderr, "fetch: destination path too long\n");
        return 2;
    }
    const tabos_tls_t connection = tabos_tls_connect(host, 443U);
    if (connection < 0) {
        fprintf(stderr, "fetch: connect %s: %s\n", host, strerror(errno));
        return 1;
    }
    char request[HOST_MAX + PATH_MAX + 128];
    const int request_size = snprintf(request, sizeof(request),
                                      "GET %s HTTP/1.0\r\nHost: %s\r\nAccept-Encoding: identity\r\n\r\n", path, host);
    FILE* output = NULL;
    int result = 1;
    if (request_size < 0 || (size_t) request_size >= sizeof(request) || send_all(connection, request, (size_t) request_size) != 0) {
        fprintf(stderr, "fetch: send: %s\n", strerror(errno));
    } else if ((output = fopen(temporary, "wb")) == NULL) {
        fprintf(stderr, "fetch: open %s: %s\n", temporary, strerror(errno));
    } else {
        uint8_t buffer[BUFFER_SIZE];
        int received = tabos_tls_receive(connection, buffer, sizeof(buffer));
        const size_t offset = received > 0 ? body_offset(buffer, (size_t) received) : 0U;
        if (received <= 0 || offset == 0U) {
            fprintf(stderr, "fetch: invalid or oversized HTTP response header\n");
        } else if (fwrite(buffer + offset, 1U, (size_t) received - offset, output) != (size_t) received - offset) {
            fprintf(stderr, "fetch: write: %s\n", strerror(errno));
        } else {
            for (;;) {
                received = tabos_tls_receive(connection, buffer, sizeof(buffer));
                if (received <= 0) {
                    break;
                }
                if (fwrite(buffer, 1U, (size_t) received, output) != (size_t) received) {
                    received = -1;
                    break;
                }
            }
            if (received == 0 && fclose(output) == 0 && rename(temporary, destination) == 0) {
                output = NULL;
                result = 0;
            } else {
                fprintf(stderr, "fetch: receive or finalize: %s\n", strerror(errno));
            }
        }
    }
    if (output != NULL) {
        (void) fclose(output);
    }
    if (result != 0) {
        (void) remove(temporary);
    }
    (void) tabos_tls_close(connection);
    if (result == 0) {
        printf("fetch: saved %s\n", destination);
    }
    return result;
}
