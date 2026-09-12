#ifndef TABOS_GUI_DRAW_H
#define TABOS_GUI_DRAW_H

#include <tabos/graphics.h>
#include <tabos/input.h>
#include <tabos/pointer.h>
#include <stddef.h>

typedef struct {
        int32_t x, y, width, height;
} tabos_gui_rect_t;
typedef struct {
        uint16_t* pixels;
        uint32_t width, height;
} tabos_gui_canvas_t;

enum {
    TABOS_GUI_FACE       = 0xc618,
    TABOS_GUI_LIGHT      = 0xffff,
    TABOS_GUI_SHADOW     = 0x632c,
    TABOS_GUI_INK        = 0x1082,
    TABOS_GUI_ACCENT     = 0x2354,
    TABOS_GUI_BACKGROUND = 0x2b8d,
    TABOS_GUI_WIDGET_MAX = 64,
};

typedef enum {
    TABOS_GUI_LABEL,
    TABOS_GUI_BUTTON,
    TABOS_GUI_CHECKBOX,
    TABOS_GUI_TEXT_FIELD,
    TABOS_GUI_SCROLLBAR,
    TABOS_GUI_LIST,
} tabos_gui_widget_kind_t;

typedef struct {
        int id;
        tabos_gui_widget_kind_t kind;
        tabos_gui_rect_t bounds;
        const char* label;
        char* text;
        size_t capacity;
        size_t cursor;
        int value;
        int maximum;
        bool disabled;
        bool multiline;
        const char* const* items;
        size_t item_count;
        size_t first_item;
} tabos_gui_widget_t;

typedef struct {
        tabos_gui_widget_t widgets[TABOS_GUI_WIDGET_MAX];
        size_t count;
        int focus;
        int pressed;
        uint32_t contact;
        tabos_device_id_t device;
        bool changed;
        size_t focus_cursor;
} tabos_gui_ui_t;

bool tabos_gui_contains(tabos_gui_rect_t rectangle, int32_t x, int32_t y);
bool tabos_gui_intersect(tabos_gui_rect_t first, tabos_gui_rect_t second, tabos_gui_rect_t* result);
void tabos_gui_fill(tabos_gui_canvas_t* canvas, tabos_gui_rect_t rectangle, uint16_t color);
void tabos_gui_text(tabos_gui_canvas_t* canvas, int32_t x, int32_t y, const char* text, uint16_t color,
                    unsigned int scale);
void tabos_gui_bevel(tabos_gui_canvas_t* canvas, tabos_gui_rect_t rectangle, bool sunken);
void tabos_gui_ui_begin(tabos_gui_ui_t* ui);
bool tabos_gui_ui_add(tabos_gui_ui_t* ui, tabos_gui_widget_t widget);
void tabos_gui_ui_draw(tabos_gui_ui_t* ui, tabos_gui_canvas_t* canvas);
int tabos_gui_ui_pointer(tabos_gui_ui_t* ui, const tabos_pointer_event_t* event);
int tabos_gui_ui_keyboard(tabos_gui_ui_t* ui, const tabos_input_event_t* event);
void tabos_gui_ui_cancel(tabos_gui_ui_t* ui);

#endif
