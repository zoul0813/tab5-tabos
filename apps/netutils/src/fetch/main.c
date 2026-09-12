#include <tabos/tls.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    HOST_MAX     = 253,
    URL_PATH_MAX = 512,
    BUFFER_SIZE  = 1024,
    HEADER_SIZE  = 4096,
};

typedef struct {
        bool has_content_length;
        size_t content_length;
} HttpResponse;

static void usage(FILE* stream)
{
    fprintf(stream, "Usage: fetch https://host[/path] [destination]\n");
}

static void default_destination(const char* path, char* destination, size_t capacity)
{
    const char* name    = strrchr(path, '/');
    name                = name == NULL ? path : name + 1;
    const char* query   = strchr(name, '?');
    const size_t length = query == NULL ? strlen(name) : (size_t) (query - name);
    if (length == 0U || (length == 1U && name[0] == '.') || (length == 2U && name[0] == '.' && name[1] == '.')) {
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
    const size_t length   = separator == NULL ? strlen(authority) : (size_t) (separator - authority);
    if (length == 0U || length > HOST_MAX || strchr(authority, ':') != NULL || strchr(authority, '@') != NULL) {
        return false;
    }
    memcpy(host, authority, length);
    host[length] = '\0';
    if (separator == NULL) {
        (void) snprintf(path, URL_PATH_MAX + 1U, "/");
    } else if (strlen(separator) > URL_PATH_MAX) {
        return false;
    } else {
        (void) snprintf(path, URL_PATH_MAX + 1U, "%s", separator);
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

static bool ascii_equal_ignore_case(const uint8_t* value, size_t length, const char* expected)
{
    if (strlen(expected) != length) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        uint8_t byte = value[index];
        if (byte >= 'A' && byte <= 'Z') {
            byte = (uint8_t) (byte + ('a' - 'A'));
        }
        if (byte != (uint8_t) expected[index]) {
            return false;
        }
    }
    return true;
}

static bool parse_content_length(const uint8_t* value, size_t length, size_t* parsed)
{
    size_t begin = 0U;
    while (begin < length && (value[begin] == ' ' || value[begin] == '\t')) {
        ++begin;
    }
    while (length > begin && (value[length - 1U] == ' ' || value[length - 1U] == '\t')) {
        --length;
    }
    if (begin == length) {
        return false;
    }
    size_t result = 0U;
    for (size_t index = begin; index < length; ++index) {
        if (value[index] < '0' || value[index] > '9') {
            return false;
        }
        const size_t digit = (size_t) (value[index] - '0');
        if (result > (SIZE_MAX - digit) / 10U) {
            return false;
        }
        result = result * 10U + digit;
    }
    *parsed = result;
    return true;
}

static bool parse_response_header(const uint8_t* header, size_t length, HttpResponse* response)
{
    const uint8_t* line_end = NULL;
    for (size_t index = 1U; index < length; ++index) {
        if (header[index - 1U] == '\r' && header[index] == '\n') {
            line_end = header + index - 1U;
            break;
        }
    }
    if (line_end == NULL || (size_t) (line_end - header) < 12U || memcmp(header, "HTTP/1.", 7U) != 0 ||
        (header[7] != '0' && header[7] != '1') || header[8] != ' ' || header[9] < '0' || header[9] > '9' ||
        header[10] < '0' || header[10] > '9' || header[11] < '0' || header[11] > '9' ||
        ((size_t) (line_end - header) > 12U && header[12] != ' ')) {
        return false;
    }
    const unsigned status =
        (unsigned) (header[9] - '0') * 100U + (unsigned) (header[10] - '0') * 10U + (unsigned) (header[11] - '0');
    if (status < 200U || status >= 300U) {
        fprintf(stderr, "fetch: HTTP status %u\n", status);
        return false;
    }

    *response           = (HttpResponse) {0};
    const uint8_t* line = line_end + 2;
    const uint8_t* end  = header + length;
    while (line < end) {
        line_end = NULL;
        for (const uint8_t* cursor = line + 1; cursor < end; ++cursor) {
            if (cursor[-1] == '\r' && cursor[0] == '\n') {
                line_end = cursor - 1;
                break;
            }
        }
        if (line_end == NULL) {
            return false;
        }
        if (line_end == line) {
            return true;
        }
        const uint8_t* colon = memchr(line, ':', (size_t) (line_end - line));
        if (colon == NULL) {
            return false;
        }
        const uint8_t* value      = colon + 1;
        const size_t name_length  = (size_t) (colon - line);
        const size_t value_length = (size_t) (line_end - value);
        if (ascii_equal_ignore_case(line, name_length, "transfer-encoding")) {
            fprintf(stderr, "fetch: unsupported HTTP Transfer-Encoding\n");
            return false;
        }
        if (ascii_equal_ignore_case(line, name_length, "content-length")) {
            size_t parsed = 0U;
            if (!parse_content_length(value, value_length, &parsed) ||
                (response->has_content_length && response->content_length != parsed)) {
                return false;
            }
            response->has_content_length = true;
            response->content_length     = parsed;
        }
        line = line_end + 2;
    }
    return false;
}

static bool write_body(FILE* output, const uint8_t* data, size_t size, const HttpResponse* response, size_t* body_size)
{
    if (response->has_content_length &&
        (*body_size > response->content_length || size > response->content_length - *body_size)) {
        fprintf(stderr, "fetch: response exceeds Content-Length\n");
        return false;
    }
    if (fwrite(data, 1U, size, output) != size) {
        fprintf(stderr, "fetch: write: %s\n", strerror(errno));
        return false;
    }
    *body_size += size;
    return true;
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
    char path[URL_PATH_MAX + 1];
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
    if (snprintf(temporary, sizeof(temporary), "%s.part", destination) < 0 ||
        strlen(destination) + 5U >= sizeof(temporary)) {
        fprintf(stderr, "fetch: destination path too long\n");
        return 2;
    }
    const tabos_tls_t connection = tabos_tls_connect(host, 443U);
    if (connection < 0) {
        fprintf(stderr, "fetch: connect %s: %s\n", host, strerror(errno));
        return 1;
    }
    char request[HOST_MAX + URL_PATH_MAX + 128];
    const int request_size = snprintf(request, sizeof(request),
                                      "GET %s HTTP/1.0\r\nHost: %s\r\nAccept-Encoding: identity\r\n\r\n", path, host);
    FILE* output           = NULL;
    int result             = 1;
    if (request_size < 0 || (size_t) request_size >= sizeof(request) ||
        send_all(connection, request, (size_t) request_size) != 0) {
        fprintf(stderr, "fetch: send: %s\n", strerror(errno));
    } else if ((output = fopen(temporary, "wb")) == NULL) {
        fprintf(stderr, "fetch: open %s: %s\n", temporary, strerror(errno));
    } else {
        uint8_t buffer[BUFFER_SIZE];
        uint8_t header[HEADER_SIZE];
        size_t header_size = 0U;
        size_t offset      = 0U;
        int received       = 0;
        while (offset == 0U && header_size < sizeof(header)) {
            const size_t remaining  = sizeof(header) - header_size;
            const uint32_t capacity = (uint32_t) (remaining < BUFFER_SIZE ? remaining : BUFFER_SIZE);
            received                = tabos_tls_receive(connection, header + header_size, capacity);
            if (received <= 0) {
                break;
            }
            header_size += (size_t) received;
            offset       = body_offset(header, header_size);
        }
        HttpResponse response;
        if (received <= 0 || offset == 0U || !parse_response_header(header, offset, &response)) {
            fprintf(stderr, "fetch: invalid or oversized HTTP response header\n");
        } else {
            size_t body_size = 0U;
            bool body_ok     = write_body(output, header + offset, header_size - offset, &response, &body_size);
            while (body_ok && (!response.has_content_length || body_size < response.content_length)) {
                received = tabos_tls_receive(connection, buffer, sizeof(buffer));
                if (received <= 0) {
                    break;
                }
                body_ok = write_body(output, buffer, (size_t) received, &response, &body_size);
            }
            if (body_ok && response.has_content_length && body_size != response.content_length) {
                fprintf(stderr, "fetch: truncated response body\n");
                body_ok = false;
            } else if (body_ok && !response.has_content_length && received < 0) {
                fprintf(stderr, "fetch: receive: %s\n", strerror(errno));
                body_ok = false;
            }
            if (body_ok) {
                const int close_result = fclose(output);
                output                 = NULL;
                if (close_result != 0) {
                    fprintf(stderr, "fetch: close %s: %s\n", temporary, strerror(errno));
                } else if (rename(temporary, destination) != 0) {
                    fprintf(stderr, "fetch: rename %s: %s\n", destination, strerror(errno));
                } else {
                    result = 0;
                }
            } else {
                result = 1;
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
