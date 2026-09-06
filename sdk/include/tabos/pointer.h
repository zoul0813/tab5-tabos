#ifndef TABOS_POINTER_H
#define TABOS_POINTER_H

#include <tabos/device.h>

#include <stdint.h>

#ifndef TABOS_POINTER_MAX_CONTACTS
#define TABOS_POINTER_MAX_CONTACTS 5
#endif

enum {
    TABOS_POINTER_BUTTON_PRIMARY     = 1U << 0U,
    TABOS_POINTER_BUTTON_SECONDARY   = 1U << 1U,
    TABOS_POINTER_BUTTON_MIDDLE      = 1U << 2U,
    TABOS_POINTER_EVENT_HAS_PRESSURE = 1U << 0U,
    TABOS_POINTER_PRESSURE_MAX       = 65535U,
};

#define TABOS_POINTER_STREAM_INVALID (-1)

typedef int32_t tabos_pointer_stream_t;

typedef enum {
    TABOS_POINTER_DOWN = 0,
    TABOS_POINTER_MOVE,
    TABOS_POINTER_UP,
    TABOS_POINTER_CANCEL,
    TABOS_POINTER_EVENT_TYPE_COUNT,
} tabos_pointer_event_type_t;

typedef struct {
        tabos_pointer_event_type_t type;
        tabos_device_id_t device_id;
        uint32_t contact_id;
        int32_t x;
        int32_t y;
        uint32_t buttons;
        uint32_t pressure;
        uint32_t flags;
} tabos_pointer_event_t;

tabos_pointer_stream_t tabos_pointer_open(tabos_device_id_t device_id);
int tabos_pointer_close(tabos_pointer_stream_t stream);
int tabos_pointer_read(tabos_pointer_stream_t stream, tabos_pointer_event_t* event);

#endif
