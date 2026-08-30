#include <tabos/internal/elf_api.h>
#include <tabos/filesystem.h>
#include <tabos/tls.h>

#include <errno.h>
#include <string.h>

const tabos_elf_api_t* tabos_runtime_api;

static int connect_tls(const char* hostname, uint32_t port)
{
    return strcmp(hostname, "example.com") == 0 && port == 443U ? 7 : -TABOS_EINVAL;
}

static int close_tls(int connection)
{
    return connection == 7 ? 0 : -TABOS_EBADF;
}

static int send_tls(int connection, const void* data, uint32_t size)
{
    return connection == 7 && data != NULL ? (int) size : -TABOS_EBADF;
}

static int receive_tls(int connection, void* data, uint32_t capacity)
{
    if (connection != 7 || data == NULL || capacity < 2U) {
        return -TABOS_EBADF;
    }
    memcpy(data, "ok", 2U);
    return 2;
}

int main(void)
{
    if (tabos_tls_connect("example.com", 443U) >= 0 || errno != ENOSYS) {
        return 1;
    }
    const tabos_elf_api_t api = {
        .tls_connect = connect_tls,
        .tls_close = close_tls,
        .tls_send = send_tls,
        .tls_receive = receive_tls,
    };
    tabos_runtime_api = &api;
    char buffer[4] = {0};
    const tabos_tls_t connection = tabos_tls_connect("example.com", 443U);
    if (connection != 7 || tabos_tls_send(connection, "GET", 3U) != 3 ||
        tabos_tls_receive(connection, buffer, sizeof(buffer)) != 2 || memcmp(buffer, "ok", 2U) != 0 ||
        tabos_tls_close(connection) != 0) {
        return 1;
    }
    if (tabos_tls_connect("", 443U) >= 0 || errno != EINVAL || tabos_tls_send(connection, NULL, 1U) >= 0 ||
        errno != EINVAL || tabos_tls_close(8) >= 0 || errno != TABOS_EBADF) {
        return 1;
    }
    return 0;
}
