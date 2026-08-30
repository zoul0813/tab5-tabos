#ifndef TABOS_WAIT_H
#define TABOS_WAIT_H

#include <tabos/network.h>

#include <stdint.h>

enum {
    TABOS_WAIT_MAX      = TABOS_SOCKET_MAX,
    TABOS_WAIT_READABLE = 1U << 0U,
    TABOS_WAIT_WRITABLE = 1U << 1U,
    TABOS_WAIT_ERROR    = 1U << 2U,
    TABOS_WAIT_HANGUP   = 1U << 3U,
};

#define TABOS_WAIT_TIMEOUT_INFINITE UINT32_MAX

typedef struct {
        tabos_socket_t socket;
        uint32_t events;
        uint32_t returned_events;
} tabos_wait_item_t;

int tabos_wait_set(tabos_wait_item_t* items, uint32_t count, uint32_t timeout_ms);

#endif
