#ifndef TABOS_GUI_H
#define TABOS_GUI_H

#include <tabos/gui_draw.h>
#include <tabos/gui_protocol.h>

typedef struct tabos_gui tabos_gui_t;
struct tabos_gui {
        tabos_gui_canvas_t canvas;
        tabos_gui_ui_t ui;
        void* user;
        void (*draw)(tabos_gui_t* gui);
        void (*action)(tabos_gui_t* gui, int widget_id);
        void (*input)(tabos_gui_t* gui, const tabos_gui_packet_t* packet, uint32_t kind);
        /* 1: close, 0: show application confirmation, -1: cancel close. */
        int (*closing)(tabos_gui_t* gui);
        /* Return false while bounded work/leases still prevent a safe pause. */
        bool (*pause)(tabos_gui_t* gui);
        void (*resume)(tabos_gui_t* gui);
        tabos_ipc_channel_t channel;
        tabos_surface_t surface;
        tabos_surface_t retired;
        uint32_t serial;
        uint32_t cancelled_input_sequence;
        bool connected;
        bool running;
        bool dirty;
        bool pending_frame;
        bool advertised;
        bool close_pending;
        bool cancel_reply_pending;
        bool error_pending;
        uint32_t error_serial;
        int pending_error;
        int last_error;
        char title[64];
};

/* Initialize callbacks/user on a zeroed object, then open within a GUI session.
 * One window per application. Drawing callbacks rebuild layout without side effects.
 * All text is CP437. Menus/dialogs compose ordinary widgets in application state. */
int tabos_gui_open(tabos_gui_t* gui, const char* title);
int tabos_gui_step(tabos_gui_t* gui, uint32_t timeout_ms);
void tabos_gui_invalidate(tabos_gui_t* gui);
void tabos_gui_close_reply(tabos_gui_t* gui, bool allow);
void tabos_gui_shutdown(tabos_gui_t* gui);
int tabos_gui_launch(tabos_gui_t* gui, const char* path);
int tabos_gui_send(tabos_ipc_channel_t channel, uint32_t kind, const tabos_gui_packet_t* packet, bool control);
int tabos_gui_receive(tabos_ipc_channel_t channel, uint32_t* kind, tabos_gui_packet_t* packet, uint32_t* sender);

#endif
