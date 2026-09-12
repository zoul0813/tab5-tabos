#ifndef TABOS_GUI_PROTOCOL_H
#define TABOS_GUI_PROTOCOL_H

#include <tabos/input.h>
#include <tabos/pointer.h>
#include <tabos/surface.h>

enum {
    TABOS_GUI_PROTOCOL_VERSION = 1U,
    TABOS_GUI_SCREEN_WIDTH     = 1280U,
    TABOS_GUI_SCREEN_HEIGHT    = 720U,
    TABOS_GUI_DOCK_HEIGHT      = 80U,
    TABOS_GUI_TITLE_HEIGHT     = 48U,
    TABOS_GUI_CONTENT_HEIGHT   = 592U,
    TABOS_GUI_WINDOW_MAX       = 8U,
    TABOS_GUI_TARGET_SIZE      = 44U,
};

typedef enum {
    TABOS_GUI_HELLO = 1,
    TABOS_GUI_FRAME,
    TABOS_GUI_ADOPT,
    TABOS_GUI_CONFIGURE,
    TABOS_GUI_POINTER,
    TABOS_GUI_KEYBOARD,
    TABOS_GUI_CANCEL_INPUT,
    TABOS_GUI_CLOSE,
    TABOS_GUI_CLOSE_CANCELLED,
    TABOS_GUI_LAUNCH,
    TABOS_GUI_ERROR,
    TABOS_GUI_WAKE,
} tabos_gui_message_kind_t;

/* One window per channel. Serial identifies adopted geometry, so queued input
 * from old geometry cannot activate controls after resize. All fields are copied.
 * FRAME transfers read access, never ownership; old surfaces live until ADOPT. */
typedef struct {
        uint32_t version;
        uint32_t serial;
        union {
                struct {
                        tabos_surface_t surface;
                        uint32_t width;
                        uint32_t height;
                        char title[64];
                } window;
                tabos_pointer_event_t pointer;
                tabos_input_event_t keyboard;
                char path[192];
                int32_t error;
        } data;
} tabos_gui_packet_t;

_Static_assert(sizeof(tabos_gui_packet_t) <= TABOS_IPC_DATA_MAX, "GUI packets must fit copied IPC");

#endif
