#include <tabos/gui_draw.h>

#include <limits.h>
#include <string.h>

extern const uint8_t tabos_gui_font[];

bool tabos_gui_contains(tabos_gui_rect_t rectangle, int32_t x, int32_t y)
{
    return rectangle.width > 0 && rectangle.height > 0 && x >= rectangle.x && y >= rectangle.y &&
           (int64_t) x < (int64_t) rectangle.x + rectangle.width &&
           (int64_t) y < (int64_t) rectangle.y + rectangle.height;
}

bool tabos_gui_intersect(tabos_gui_rect_t first, tabos_gui_rect_t second, tabos_gui_rect_t* result)
{
    if (result == NULL || first.width <= 0 || first.height <= 0 || second.width <= 0 || second.height <= 0) {
        return false;
    }
    const int64_t left          = first.x > second.x ? first.x : second.x;
    const int64_t top           = first.y > second.y ? first.y : second.y;
    const int64_t first_right   = (int64_t) first.x + first.width;
    const int64_t second_right  = (int64_t) second.x + second.width;
    const int64_t first_bottom  = (int64_t) first.y + first.height;
    const int64_t second_bottom = (int64_t) second.y + second.height;
    const int64_t right         = first_right < second_right ? first_right : second_right;
    const int64_t bottom        = first_bottom < second_bottom ? first_bottom : second_bottom;
    if (right <= left || bottom <= top || right - left > INT32_MAX || bottom - top > INT32_MAX) {
        return false;
    }
    *result = (tabos_gui_rect_t) {(int32_t) left, (int32_t) top, (int32_t) (right - left), (int32_t) (bottom - top)};
    return true;
}

void tabos_gui_fill(tabos_gui_canvas_t* canvas, tabos_gui_rect_t rectangle, uint16_t color)
{
    tabos_gui_rect_t clipped;
    if (canvas == NULL || canvas->pixels == NULL || canvas->width > 1280U || canvas->height > 720U ||
        !tabos_gui_intersect(rectangle, (tabos_gui_rect_t) {0, 0, (int32_t) canvas->width, (int32_t) canvas->height},
                             &clipped)) {
        return;
    }
    for (int32_t y = clipped.y; y < clipped.y + clipped.height; ++y) {
        uint16_t* pixels = canvas->pixels + (size_t) y * canvas->width + (size_t) clipped.x;
        for (int32_t x = 0; x < clipped.width; ++x) {
            pixels[x] = color;
        }
    }
}

void tabos_gui_text(tabos_gui_canvas_t* canvas, int32_t x, int32_t y, const char* text, uint16_t color,
                    unsigned int scale)
{
    if (canvas == NULL || text == NULL || scale == 0U || scale > 4U) {
        return;
    }
    int64_t position = x;
    for (size_t index = 0U; text[index] != '\0' && index < 4096U; ++index) {
        if (position >= canvas->width) {
            break;
        }
        const uint8_t* glyph = tabos_gui_font + (size_t) (uint8_t) text[index] * 12U;
        for (unsigned int row = 0U; row < 12U; ++row) {
            for (unsigned int column = 0U; column < 8U; ++column) {
                if ((glyph[row] & (0x80U >> column)) != 0U) {
                    const int64_t px = position + column * scale;
                    const int64_t py = (int64_t) y + row * scale;
                    if (px >= INT32_MIN && px <= INT32_MAX && py >= INT32_MIN && py <= INT32_MAX) {
                        tabos_gui_fill(
                            canvas, (tabos_gui_rect_t) {(int32_t) px, (int32_t) py, (int32_t) scale, (int32_t) scale},
                            color);
                    }
                }
            }
        }
        position += 8U * scale;
    }
}

void tabos_gui_bevel(tabos_gui_canvas_t* canvas, tabos_gui_rect_t rectangle, bool sunken)
{
    if (rectangle.width < 4 || rectangle.height < 4 || rectangle.x < -1280 || rectangle.y < -720 ||
        rectangle.x > 1280 || rectangle.y > 720 || rectangle.width > 1280 || rectangle.height > 720) {
        return;
    }
    const uint16_t light = sunken ? TABOS_GUI_SHADOW : TABOS_GUI_LIGHT;
    const uint16_t dark  = sunken ? TABOS_GUI_LIGHT : TABOS_GUI_SHADOW;
    tabos_gui_fill(canvas, rectangle, TABOS_GUI_FACE);
    tabos_gui_fill(canvas, (tabos_gui_rect_t) {rectangle.x, rectangle.y, rectangle.width, 2}, light);
    tabos_gui_fill(canvas, (tabos_gui_rect_t) {rectangle.x, rectangle.y, 2, rectangle.height}, light);
    tabos_gui_fill(canvas, (tabos_gui_rect_t) {rectangle.x, rectangle.y + rectangle.height - 2, rectangle.width, 2},
                   dark);
    tabos_gui_fill(canvas, (tabos_gui_rect_t) {rectangle.x + rectangle.width - 2, rectangle.y, 2, rectangle.height},
                   dark);
}

static tabos_gui_widget_t* find_widget(tabos_gui_ui_t* ui, int id)
{
    for (size_t index = 0U; index < ui->count; ++index) {
        if (ui->widgets[index].id == id) {
            return &ui->widgets[index];
        }
    }
    return NULL;
}

void tabos_gui_ui_begin(tabos_gui_ui_t* ui)
{
    const tabos_gui_widget_t* focused = find_widget(ui, ui->focus);
    if (focused != NULL && focused->kind == TABOS_GUI_TEXT_FIELD) {
        ui->focus_cursor = focused->cursor;
    }
    ui->count = 0U;
}

bool tabos_gui_ui_add(tabos_gui_ui_t* ui, tabos_gui_widget_t widget)
{
    if (ui == NULL || ui->count >= TABOS_GUI_WIDGET_MAX || widget.id <= 0 || find_widget(ui, widget.id) != NULL ||
        widget.bounds.x < -1280 || widget.bounds.x > 1280 || widget.bounds.y < -720 || widget.bounds.y > 720 ||
        widget.bounds.width <= 0 || widget.bounds.width > 1280 || widget.bounds.height <= 0 ||
        widget.bounds.height > 720) {
        return false;
    }
    if (widget.id == ui->focus && widget.kind == TABOS_GUI_TEXT_FIELD) {
        widget.cursor = ui->focus_cursor;
    }
    if (widget.text != NULL && widget.capacity > 0U) {
        widget.text[widget.capacity - 1U] = '\0';
        const size_t length               = strlen(widget.text);
        if (widget.cursor > length) {
            widget.cursor = length;
        }
    }
    ui->widgets[ui->count++] = widget;
    return true;
}

static void field_draw(tabos_gui_canvas_t* canvas, const tabos_gui_widget_t* widget)
{
    if (widget->text == NULL || widget->capacity == 0U || widget->bounds.width < 24 || widget->bounds.height < 28) {
        return;
    }
    const size_t columns = (size_t) (widget->bounds.width - 16) / 16U;
    const size_t rows    = widget->multiline ? (size_t) (widget->bounds.height - 8) / 28U : 1U;
    size_t line          = 0U;
    size_t column        = 0U;
    size_t start         = 0U;
    if (!widget->multiline && widget->cursor >= columns) {
        start = widget->cursor - columns + 1U;
    }
    for (size_t index = start; widget->text[index] != '\0' && index < widget->capacity && line < rows; ++index) {
        const char character[2] = {widget->text[index], '\0'};
        if (character[0] == '\n') {
            ++line;
            column = 0U;
            continue;
        }
        tabos_gui_text(canvas, widget->bounds.x + 8 + (int32_t) column * 16, widget->bounds.y + 4 + (int32_t) line * 28,
                       character, TABOS_GUI_INK, 2U);
        if (++column == columns) {
            column = 0U;
            ++line;
        }
    }
}

static void draw_label(tabos_gui_canvas_t* canvas, int32_t x, int32_t y, int32_t width, const char* text,
                       uint16_t color)
{
    if (width <= 0 || text == NULL) {
        return;
    }
    char visible[81];
    size_t count = (size_t) width / 16U;
    if (count > sizeof(visible) - 1U) {
        count = sizeof(visible) - 1U;
    }
    size_t used = 0U;
    while (used < count && text[used] != '\0') {
        visible[used] = text[used];
        ++used;
    }
    visible[used] = '\0';
    tabos_gui_text(canvas, x, y, visible, color, 2U);
}

void tabos_gui_ui_draw(tabos_gui_ui_t* ui, tabos_gui_canvas_t* canvas)
{
    for (size_t index = 0U; index < ui->count; ++index) {
        tabos_gui_widget_t* widget = &ui->widgets[index];
        const tabos_gui_rect_t box = widget->bounds;
        const uint16_t ink         = widget->disabled ? TABOS_GUI_SHADOW : TABOS_GUI_INK;
        if (widget->kind != TABOS_GUI_LABEL) {
            tabos_gui_bevel(canvas, box,
                            ui->pressed == widget->id || widget->kind == TABOS_GUI_TEXT_FIELD ||
                                widget->kind == TABOS_GUI_LIST);
        }
        if (ui->focus == widget->id) {
            tabos_gui_fill(canvas, (tabos_gui_rect_t) {box.x + 4, box.y + box.height - 5, box.width - 8, 2},
                           TABOS_GUI_ACCENT);
        }
        if (widget->kind == TABOS_GUI_TEXT_FIELD) {
            field_draw(canvas, widget);
        } else if (widget->kind == TABOS_GUI_SCROLLBAR) {
            const int travel  = box.height > 44 ? box.height - 44 : 0;
            const int maximum = widget->maximum > 0 ? widget->maximum : 1;
            int value         = widget->value;
            if (value < 0) {
                value = 0;
            }
            if (value > maximum) {
                value = maximum;
            }
            const int offset = (int) ((int64_t) travel * value / maximum);
            tabos_gui_bevel(canvas, (tabos_gui_rect_t) {box.x + 2, box.y + offset, box.width - 4, 44}, false);
        } else if (widget->kind == TABOS_GUI_LIST) {
            for (size_t row = 0U; row < (size_t) (box.height > 8 ? box.height - 8 : 0) / 44U; ++row) {
                const size_t item = widget->first_item + row;
                if (widget->items == NULL || item >= widget->item_count) {
                    break;
                }
                const int32_t y = box.y + 4 + (int32_t) row * 44;
                if (widget->value >= 0 && item == (size_t) widget->value) {
                    tabos_gui_fill(canvas, (tabos_gui_rect_t) {box.x + 4, y, box.width - 8, 44}, TABOS_GUI_ACCENT);
                }
                draw_label(canvas, box.x + 12, y + 10, box.width - 24, widget->items[item],
                           item == (size_t) widget->value ? TABOS_GUI_LIGHT : ink);
            }
        } else {
            int32_t x = box.x + 12;
            if (widget->kind == TABOS_GUI_CHECKBOX) {
                tabos_gui_bevel(canvas, (tabos_gui_rect_t) {box.x + 8, box.y + 8, 28, 28}, true);
                if (widget->value != 0) {
                    tabos_gui_fill(canvas, (tabos_gui_rect_t) {box.x + 14, box.y + 14, 16, 16}, TABOS_GUI_ACCENT);
                }
                x += 36;
            }
            draw_label(canvas, x, box.y + (box.height - 24) / 2, box.width - (x - box.x) - 8, widget->label, ink);
        }
    }
}

void tabos_gui_ui_cancel(tabos_gui_ui_t* ui)
{
    ui->changed = ui->pressed != 0;
    ui->pressed = 0;
}

int tabos_gui_ui_pointer(tabos_gui_ui_t* ui, const tabos_pointer_event_t* event)
{
    if (ui == NULL || event == NULL) {
        return 0;
    }
    ui->changed = false;
    if (event->type == TABOS_POINTER_CANCEL) {
        if (event->contact_id == ui->contact && event->device_id == ui->device) {
            tabos_gui_ui_cancel(ui);
        }
        return 0;
    }
    if (event->type == TABOS_POINTER_DOWN && ui->pressed == 0) {
        for (size_t index = ui->count; index > 0U; --index) {
            tabos_gui_widget_t* widget = &ui->widgets[index - 1U];
            if (!widget->disabled && widget->kind != TABOS_GUI_LABEL &&
                tabos_gui_contains(widget->bounds, event->x, event->y)) {
                ui->pressed = widget->id;
                ui->focus   = widget->id;
                ui->contact = event->contact_id;
                ui->device  = event->device_id;
                ui->changed = true;
                break;
            }
        }
    }
    if (ui->pressed == 0 || event->contact_id != ui->contact || event->device_id != ui->device) {
        return 0;
    }
    tabos_gui_widget_t* widget = find_widget(ui, ui->pressed);
    if (widget == NULL) {
        tabos_gui_ui_cancel(ui);
        return 0;
    }
    if (widget->kind == TABOS_GUI_SCROLLBAR) {
        int64_t position = (int64_t) event->y - widget->bounds.y - 22;
        const int travel = widget->bounds.height > 44 ? widget->bounds.height - 44 : 1;
        if (position < 0) {
            position = 0;
        }
        if (position > travel) {
            position = travel;
        }
        widget->value = (int) (position * widget->maximum / travel);
        ui->changed   = true;
        if (event->type == TABOS_POINTER_UP) {
            ui->pressed = 0;
        }
        return widget->id;
    }
    if (event->type != TABOS_POINTER_UP) {
        return 0;
    }
    ui->pressed = 0;
    ui->changed = true;
    if (!tabos_gui_contains(widget->bounds, event->x, event->y)) {
        return 0;
    }
    if (widget->kind == TABOS_GUI_CHECKBOX) {
        widget->value = !widget->value;
    }
    if (widget->kind == TABOS_GUI_LIST) {
        const int row     = (event->y - widget->bounds.y - 4) / 44;
        const size_t item = widget->first_item + (size_t) (row > 0 ? row : 0);
        if (item >= widget->item_count) {
            return 0;
        }
        widget->value = (int) item;
    }
    return widget->id;
}

int tabos_gui_ui_keyboard(tabos_gui_ui_t* ui, const tabos_input_event_t* event)
{
    if (ui == NULL || event == NULL) {
        return 0;
    }
    ui->changed = false;
    if (event->type == TABOS_INPUT_KEY_DOWN && event->key == TABOS_KEY_TAB) {
        if (ui->count == 0U) {
            return 0;
        }
        size_t current = (event->modifiers & TABOS_MODIFIER_SHIFT) != 0U ? 0U : ui->count - 1U;
        for (size_t index = 0U; index < ui->count; ++index) {
            if (ui->widgets[index].id == ui->focus) {
                current = index;
            }
        }
        for (size_t step = 1U; step <= ui->count; ++step) {
            size_t index = (current + step) % ui->count;
            if ((event->modifiers & TABOS_MODIFIER_SHIFT) != 0U) {
                index = (current + ui->count - step) % ui->count;
            }
            if (!ui->widgets[index].disabled && ui->widgets[index].kind != TABOS_GUI_LABEL) {
                ui->focus   = ui->widgets[index].id;
                ui->changed = true;
                break;
            }
        }
        return 0;
    }
    tabos_gui_widget_t* widget = find_widget(ui, ui->focus);
    if (widget == NULL || widget->disabled) {
        return 0;
    }
    if (widget->kind != TABOS_GUI_TEXT_FIELD) {
        if (event->type == TABOS_INPUT_KEY_DOWN && !event->repeat &&
            (event->key == TABOS_KEY_ENTER || event->key == TABOS_KEY_SPACE)) {
            if (widget->kind == TABOS_GUI_CHECKBOX) {
                widget->value = !widget->value;
            }
            ui->changed = true;
            return widget->id;
        }
        return 0;
    }
    if (widget->text == NULL || widget->capacity == 0U) {
        return 0;
    }
    bool text_changed = false;
    size_t length     = strlen(widget->text);
    if (widget->cursor > length) {
        widget->cursor = length;
    }
    char insertion[TABOS_INPUT_TEXT_MAX_BYTES + 1U] = {0};
    if (event->type == TABOS_INPUT_TEXT &&
        (event->modifiers & (TABOS_MODIFIER_CONTROL | TABOS_MODIFIER_ALT | TABOS_MODIFIER_GUI)) == 0U) {
        size_t used = 0U;
        for (size_t index = 0U; index < TABOS_INPUT_TEXT_MAX_BYTES && event->text[index] != '\0'; ++index) {
            const uint8_t value = (uint8_t) event->text[index];
            if (value >= 32U && value != 127U) {
                insertion[used++] = (char) value;
            }
        }
    } else if (event->type == TABOS_INPUT_KEY_DOWN) {
        if (event->key == TABOS_KEY_BACKSPACE && widget->cursor > 0U) {
            memmove(widget->text + widget->cursor - 1U, widget->text + widget->cursor, length - widget->cursor + 1U);
            --widget->cursor;
            ui->changed  = true;
            text_changed = true;
        } else if (event->key == TABOS_KEY_DELETE && widget->cursor < length) {
            memmove(widget->text + widget->cursor, widget->text + widget->cursor + 1U, length - widget->cursor);
            ui->changed  = true;
            text_changed = true;
        } else if (event->key == TABOS_KEY_LEFT && widget->cursor > 0U) {
            --widget->cursor;
            ui->changed = true;
        } else if (event->key == TABOS_KEY_RIGHT && widget->cursor < length) {
            ++widget->cursor;
            ui->changed = true;
        } else if (event->key == TABOS_KEY_HOME) {
            widget->cursor = 0U;
            ui->changed    = true;
        } else if (event->key == TABOS_KEY_END) {
            widget->cursor = length;
            ui->changed    = true;
        } else if (event->key == TABOS_KEY_ENTER && widget->multiline) {
            insertion[0] = '\n';
        }
    }
    const size_t added = strlen(insertion);
    if (added > 0U && added < widget->capacity - length) {
        memmove(widget->text + widget->cursor + added, widget->text + widget->cursor, length - widget->cursor + 1U);
        memcpy(widget->text + widget->cursor, insertion, added);
        widget->cursor += added;
        ui->changed     = true;
        text_changed    = true;
    }
    return text_changed ? widget->id : 0;
}
