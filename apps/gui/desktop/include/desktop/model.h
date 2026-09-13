#ifndef DESKTOP_MODEL_H
#define DESKTOP_MODEL_H

#include <tabos/gui.h>

typedef struct {
        bool occupied;
        bool minimized;
        bool maximized;
        bool pending;
        bool pending_maximized;
        bool closing;
        int pid;
        tabos_ipc_channel_t channel;
        tabos_surface_t surface;
        uint32_t serial;
        uint32_t requested_serial;
        uint32_t input_sequence;
        uint32_t cancelled_input_sequence;
        tabos_gui_rect_t bounds;
        tabos_gui_rect_t restored;
        tabos_gui_rect_t proposed;
        char title[64];
} desktop_window_t;

typedef struct {
        desktop_window_t windows[TABOS_GUI_WINDOW_MAX];
        unsigned int stack[TABOS_GUI_WINDOW_MAX];
        size_t count;
        int focus;
        tabos_gui_rect_t damage;
        bool damaged;
        bool dragging;
        bool resizing;
        int drag_window;
        int32_t drag_x, drag_y;
        tabos_gui_rect_t drag_origin;
        tabos_gui_rect_t outline;
} desktop_model_t;

void desktop_model_init(desktop_model_t* model);
int desktop_model_add(desktop_model_t* model, int pid, const char* title);
void desktop_model_remove(desktop_model_t* model, unsigned int slot);
void desktop_model_focus(desktop_model_t* model, unsigned int slot);
void desktop_model_minimize(desktop_model_t* model, unsigned int slot);
int desktop_model_hit(const desktop_model_t* model, int32_t x, int32_t y);
void desktop_model_damage(desktop_model_t* model, tabos_gui_rect_t rectangle);
bool desktop_model_configure(desktop_model_t* model, unsigned int slot, tabos_gui_rect_t bounds, bool maximized);
bool desktop_model_adopt(desktop_model_t* model, unsigned int slot, uint32_t serial, tabos_surface_t surface,
                         uint32_t width, uint32_t height);
void desktop_model_resize_failed(desktop_model_t* model, unsigned int slot, uint32_t serial);
void desktop_model_drag_begin(desktop_model_t* model, unsigned int slot, int32_t x, int32_t y, bool resize);
void desktop_model_drag_move(desktop_model_t* model, int32_t x, int32_t y);
bool desktop_model_drag_end(desktop_model_t* model, bool cancel);
typedef int (*desktop_surface_reader_t)(void* user, tabos_surface_t surface, uint32_t width, uint32_t height,
                                        uint16_t* pixels);
bool desktop_render(desktop_model_t* model, tabos_gui_canvas_t* canvas, uint16_t* scratch, size_t scratch_pixels,
                    desktop_surface_reader_t read_surface, void* user);

#endif
