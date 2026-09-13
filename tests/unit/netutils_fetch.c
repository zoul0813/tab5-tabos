#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <tabos/tls.h>

static const uint8_t* response_data;
static size_t response_size;
static size_t response_offset;
static size_t first_receive_size;
static int receive_error;
static int close_error;
static int rename_error;
static unsigned close_calls;
static char destination_path[128];
static char temporary_path[sizeof(destination_path) + 5U];

tabos_tls_t tabos_tls_connect(const char* host, uint16_t port)
{
    (void) host;
    (void) port;
    response_offset = 0U;
    return 1;
}

int tabos_tls_send(tabos_tls_t connection, const void* data, uint32_t size)
{
    (void) connection;
    (void) data;
    return (int) size;
}

int tabos_tls_receive(tabos_tls_t connection, void* data, uint32_t capacity)
{
    (void) connection;
    if (response_offset == response_size) {
        if (receive_error != 0) {
            errno = receive_error;
            return -1;
        }
        return 0;
    }
    size_t size = response_size - response_offset;
    if (response_offset == 0U && size > first_receive_size) {
        size = first_receive_size;
    }
    if (size > capacity) {
        size = capacity;
    }
    memcpy(data, response_data + response_offset, size);
    response_offset += size;
    return (int) size;
}

int tabos_tls_close(tabos_tls_t connection)
{
    (void) connection;
    return 0;
}

static int fetch_test_fclose(FILE* stream)
{
    ++close_calls;
    const int result = fclose(stream);
    if (close_error != 0) {
        errno = close_error;
        return EOF;
    }
    return result;
}

static int fetch_test_rename(const char* source, const char* destination)
{
    if (rename_error != 0) {
        errno = rename_error;
        return -1;
    }
    return rename(source, destination);
}

#define fclose fetch_test_fclose
#define main   tabos_fetch_main
#define rename fetch_test_rename
#include "apps/netutils/src/fetch/main.c"
#undef fclose
#undef main
#undef rename

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "netutils fetch test failed: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void write_file(const char* path, const char* contents)
{
    FILE* file = fopen(path, "wb");
    check(file != NULL, "open fixture file");
    const size_t size = strlen(contents);
    check(fwrite(contents, 1U, size, file) == size, "write fixture file");
    check(fclose(file) == 0, "close fixture file");
}

static void check_file(const char* path, const char* expected)
{
    FILE* file = fopen(path, "rb");
    check(file != NULL, "open result file");
    char buffer[64];
    const size_t size = fread(buffer, 1U, sizeof(buffer), file);
    check(!ferror(file), "read result file");
    check(fclose(file) == 0, "close result file");
    check(size == strlen(expected) && memcmp(buffer, expected, size) == 0, "result file contents");
}

static int run_fetch(const uint8_t* response, size_t size, size_t first_size)
{
    response_data      = response;
    response_size      = size;
    first_receive_size = first_size;
    close_calls        = 0U;
    char* arguments[]  = {"fetch", "https://example.com/file", destination_path};
    return tabos_fetch_main(3, arguments);
}

static void reset_destination(void)
{
    write_file(destination_path, "original");
    (void) remove(temporary_path);
    close_error   = 0;
    rename_error  = 0;
    receive_error = 0;
}

static void check_failure_cleanup(void)
{
    check_file(destination_path, "original");
    errno = 0;
    check(access(temporary_path, F_OK) != 0 && errno == ENOENT, "remove failed download temporary file");
}

int main(void)
{
    check(snprintf(destination_path, sizeof(destination_path), "/tmp/tabos-fetch-%ld.bin", (long) getpid()) > 0,
          "format destination path");
    check(snprintf(temporary_path, sizeof(temporary_path), "%s.part", destination_path) > 0, "format temporary path");

    static const uint8_t success[] = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nX-Test: yes\r\n\r\nbody";
    const size_t header_size       = body_offset(success, sizeof(success) - 1U);
    check(header_size != 0U, "find fixture header boundary");
    for (size_t split = 1U; split <= header_size; ++split) {
        reset_destination();
        check(run_fetch(success, sizeof(success) - 1U, split) == 0, "accept split response header");
        check(close_calls == 1U, "close successful output exactly once");
        check_file(destination_path, "body");
    }

    reset_destination();
    static const uint8_t close_delimited[] = "HTTP/1.0 200 OK\r\n\r\nclose body";
    check(run_fetch(close_delimited, sizeof(close_delimited) - 1U, 2U) == 0, "accept close-delimited body");
    check_file(destination_path, "close body");

    reset_destination();
    static const uint8_t not_found[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 5\r\n\r\nerror";
    check(run_fetch(not_found, sizeof(not_found) - 1U, sizeof(not_found)) != 0, "reject error status");
    check_failure_cleanup();

    reset_destination();
    static const uint8_t redirect[] = "HTTP/1.1 302 Found\r\nContent-Length: 4\r\n\r\nnext";
    check(run_fetch(redirect, sizeof(redirect) - 1U, sizeof(redirect)) != 0, "reject redirect status");
    check_failure_cleanup();

    reset_destination();
    static const uint8_t truncated[] = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nabc";
    check(run_fetch(truncated, sizeof(truncated) - 1U, sizeof(truncated)) != 0, "reject truncated body");
    check_failure_cleanup();

    reset_destination();
    static const uint8_t excessive[] = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nbody";
    check(run_fetch(excessive, sizeof(excessive) - 1U, sizeof(excessive)) != 0, "reject excessive body");
    check_failure_cleanup();

    reset_destination();
    static const uint8_t chunked[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n";
    check(run_fetch(chunked, sizeof(chunked) - 1U, sizeof(chunked)) != 0, "reject transfer coding");
    check_failure_cleanup();

    reset_destination();
    uint8_t oversized[HEADER_SIZE];
    memset(oversized, 'x', sizeof(oversized));
    check(run_fetch(oversized, sizeof(oversized), 1U) != 0, "reject oversized header");
    check_failure_cleanup();

    reset_destination();
    close_error = EIO;
    check(run_fetch(success, sizeof(success) - 1U, sizeof(success)) != 0, "report close failure");
    check(close_calls == 1U, "do not close failed stream twice");
    check_failure_cleanup();

    reset_destination();
    rename_error = EACCES;
    check(run_fetch(success, sizeof(success) - 1U, sizeof(success)) != 0, "report rename failure");
    check(close_calls == 1U, "do not close stream again after rename failure");
    check_failure_cleanup();

    reset_destination();
    receive_error = ECONNRESET;
    check(run_fetch(close_delimited, sizeof(close_delimited) - 1U, sizeof(close_delimited)) != 0,
          "reject failed close-delimited receive");
    check_failure_cleanup();

    (void) remove(destination_path);
    (void) remove(temporary_path);
    return EXIT_SUCCESS;
}
