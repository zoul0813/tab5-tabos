#include <tabos/internal/elf_api.h>
#include <tabos/wait.h>

#include <errno.h>
#include <stddef.h>

extern const tabos_elf_api_t* tabos_runtime_api;

tabos_wait_source_t tabos_socket_wait_source(tabos_socket_t socket)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->socket_wait_source == NULL) {
        errno = ENOSYS;
        return TABOS_WAIT_SOURCE_INVALID;
    }
    const int result = tabos_runtime_api->socket_wait_source(socket);
    if (result < 0) {
        errno = -result;
        return TABOS_WAIT_SOURCE_INVALID;
    }
    return result;
}

tabos_wait_source_t tabos_device_subscription_wait_source(tabos_device_subscription_t subscription)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->device_subscription_wait_source == NULL) {
        errno = ENOSYS;
        return TABOS_WAIT_SOURCE_INVALID;
    }
    const int result = tabos_runtime_api->device_subscription_wait_source(subscription);
    if (result < 0) {
        errno = -result;
        return TABOS_WAIT_SOURCE_INVALID;
    }
    return result;
}

int tabos_wait(tabos_wait_item_t* items, uint32_t count, uint32_t timeout_ms)
{
    const uint32_t valid_events =
        TABOS_WAIT_READABLE | TABOS_WAIT_WRITABLE | TABOS_WAIT_STATE_CHANGED | TABOS_WAIT_ERROR | TABOS_WAIT_HANGUP;
    if (items == NULL || count == 0U || count > TABOS_WAIT_MAX) {
        errno = EINVAL;
        return -1;
    }
    tabos_elf_wait_item_t transport[TABOS_WAIT_MAX];
    for (uint32_t index = 0U; index < count; ++index) {
        if (items[index].events == 0U || (items[index].events & ~valid_events) != 0U) {
            errno = EINVAL;
            return -1;
        }
        items[index].returned_events = 0U;
        transport[index]             = (tabos_elf_wait_item_t) {
                        .source = items[index].source,
                        .events = items[index].events,
        };
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->wait == NULL) {
        errno = ENOSYS;
        return -1;
    }
    const int result = tabos_runtime_api->wait(transport, count, timeout_ms);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    for (uint32_t index = 0U; index < count; ++index) {
        items[index].returned_events = transport[index].returned_events;
    }
    return result;
}
