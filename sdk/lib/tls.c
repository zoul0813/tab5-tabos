#include <tabos/internal/elf_api.h>
#include <tabos/tls.h>

#include <errno.h>
#include <stddef.h>
#include <string.h>

extern const tabos_elf_api_t* tabos_runtime_api;

static int api_result(int result)
{
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return result;
}

tabos_tls_t tabos_tls_connect(const char* hostname, uint16_t port)
{
    if (hostname == NULL || hostname[0] == '\0' || strlen(hostname) > 253U || port == 0U ||
        tabos_runtime_api == NULL || tabos_runtime_api->tls_connect == NULL) {
        errno = tabos_runtime_api == NULL || tabos_runtime_api->tls_connect == NULL ? ENOSYS : EINVAL;
        return -1;
    }
    return api_result(tabos_runtime_api->tls_connect(hostname, port));
}

int tabos_tls_close(tabos_tls_t connection)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->tls_close == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->tls_close(connection));
}

int tabos_tls_send(tabos_tls_t connection, const void* data, uint32_t size)
{
    if (data == NULL || size == 0U || size > 1024U || tabos_runtime_api == NULL || tabos_runtime_api->tls_send == NULL) {
        errno = tabos_runtime_api == NULL || tabos_runtime_api->tls_send == NULL ? ENOSYS : EINVAL;
        return -1;
    }
    return api_result(tabos_runtime_api->tls_send(connection, data, size));
}

int tabos_tls_receive(tabos_tls_t connection, void* data, uint32_t capacity)
{
    if (data == NULL || capacity == 0U || capacity > 1024U || tabos_runtime_api == NULL ||
        tabos_runtime_api->tls_receive == NULL) {
        errno = tabos_runtime_api == NULL || tabos_runtime_api->tls_receive == NULL ? ENOSYS : EINVAL;
        return -1;
    }
    return api_result(tabos_runtime_api->tls_receive(connection, data, capacity));
}
