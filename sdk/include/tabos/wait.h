#ifndef TABOS_WAIT_H
#define TABOS_WAIT_H

#include <tabos/network.h>
#include <tabos/device.h>

#include <stdint.h>

enum {
    TABOS_WAIT_MAX           = 16,
    TABOS_WAIT_READABLE      = 1U << 0U,
    TABOS_WAIT_WRITABLE      = 1U << 1U,
    TABOS_WAIT_STATE_CHANGED = 1U << 2U,
    TABOS_WAIT_ERROR         = 1U << 3U,
    TABOS_WAIT_HANGUP        = 1U << 4U,
};

#define TABOS_WAIT_TIMEOUT_INFINITE UINT32_MAX
#define TABOS_WAIT_SOURCE_INVALID   (-1)

typedef int32_t tabos_wait_source_t;

typedef struct {
        tabos_wait_source_t source;
        uint32_t events;
        uint32_t returned_events;
} tabos_wait_item_t;

tabos_wait_source_t tabos_socket_wait_source(tabos_socket_t socket);
tabos_wait_source_t tabos_device_subscription_wait_source(tabos_device_subscription_t subscription);
int tabos_wait(tabos_wait_item_t* items, uint32_t count, uint32_t timeout_ms);

#endif
