#include <desktop/model.h>

#include <string.h>

static void outline(tabos_gui_canvas_t* canvas, tabos_gui_rect_t box, uint16_t color)
{
    tabos_gui_fill(canvas, (tabos_gui_rect_t) {box.x, box.y, box.width, 3}, color);
    tabos_gui_fill(canvas, (tabos_gui_rect_t) {box.x, box.y, 3, box.height}, color);
    tabos_gui_fill(canvas, (tabos_gui_rect_t) {box.x, box.y + box.height - 3, box.width, 3}, color);
    tabos_gui_fill(canvas, (tabos_gui_rect_t) {box.x + box.width - 3, box.y, 3, box.height}, color);
}

bool desktop_render(desktop_model_t* model, tabos_gui_canvas_t* canvas, uint16_t* scratch, size_t scratch_pixels,
                    desktop_surface_reader_t read_surface, void* user)
{
    if (model == NULL || canvas == NULL || canvas->pixels == NULL || scratch == NULL || read_surface == NULL ||
        canvas->width != 1280U || canvas->height != 720U) {
        return false;
    }
    if (!model->damaged) {
        return true;
    }
    canvas->clip = model->damage;
    tabos_gui_fill(canvas, model->damage, TABOS_GUI_BACKGROUND);
    bool success = true;
    for (size_t index = 0U; index < model->count; ++index) {
        const unsigned int slot        = model->stack[index];
        const desktop_window_t* window = &model->windows[slot];
        const tabos_gui_rect_t box     = window->bounds;
        tabos_gui_rect_t visible;
        if (window->minimized || !tabos_gui_intersect(box, model->damage, &visible)) {
            continue;
        }
        tabos_gui_bevel(canvas, box, false);
        const uint16_t title_color = model->focus == (int) slot ? TABOS_GUI_ACCENT : TABOS_GUI_SHADOW;
        tabos_gui_fill(canvas, (tabos_gui_rect_t) {box.x + 2, box.y + 2, box.width - 4, 44}, title_color);
        char title[81];
        const size_t count = (size_t) (box.width - 168) / 16U;
        size_t length      = 0U;
        while (length < count && length < sizeof(window->title) && window->title[length] != '\0') {
            title[length] = window->title[length];
            ++length;
        }
        title[length] = '\0';
        tabos_gui_text(canvas, box.x + 12, box.y + 12, title, TABOS_GUI_LIGHT, 2U);
        static const char* const controls[] = {"X", "+", "_"};
        for (int control = 0; control < 3; ++control) {
            const tabos_gui_rect_t button = {box.x + box.width - 48 * (control + 1), box.y + 2, 46, 44};
            tabos_gui_bevel(canvas, button, false);
            tabos_gui_text(canvas, button.x + 15, button.y + 10, controls[control], TABOS_GUI_INK, 2U);
        }
        const uint32_t width  = (uint32_t) box.width;
        const uint32_t height = (uint32_t) box.height - TABOS_GUI_TITLE_HEIGHT;
        if (window->surface > 0 && (size_t) width * height <= scratch_pixels &&
            read_surface(user, window->surface, width, height, scratch) == 0) {
            const tabos_gui_rect_t content = {box.x, box.y + TABOS_GUI_TITLE_HEIGHT, box.width, (int32_t) height};
            if (tabos_gui_intersect(content, model->damage, &visible)) {
                for (int32_t row = visible.y; row < visible.y + visible.height; ++row) {
                    memcpy(canvas->pixels + (size_t) row * canvas->width + (size_t) visible.x,
                           scratch + (size_t) (row - content.y) * width + (size_t) (visible.x - content.x),
                           (size_t) visible.width * sizeof(uint16_t));
                }
            }
        } else if (window->surface > 0) {
            success = false;
        } else {
            tabos_gui_text(canvas, box.x + 24, box.y + 72, "Starting...", TABOS_GUI_INK, 2U);
        }
        if (!window->maximized) {
            const tabos_gui_rect_t grip = {box.x + box.width - 44, box.y + box.height - 44, 44, 44};
            tabos_gui_bevel(canvas, grip, false);
            tabos_gui_text(canvas, grip.x + 12, grip.y + 10, "/", TABOS_GUI_INK, 2U);
        }
    }
    if (model->dragging && model->resizing) {
        outline(canvas, model->outline, TABOS_GUI_LIGHT);
    }
    tabos_gui_bevel(canvas, (tabos_gui_rect_t) {0, 640, 1280, 80}, false);
    return success;
}
