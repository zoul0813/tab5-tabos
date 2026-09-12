#include <desktop/model.h>

#include <limits.h>
#include <string.h>

static const tabos_gui_rect_t screen           = {0, 0, TABOS_GUI_SCREEN_WIDTH, TABOS_GUI_SCREEN_HEIGHT};
static const tabos_gui_rect_t maximized_bounds = {0, 0, TABOS_GUI_SCREEN_WIDTH,
                                                  TABOS_GUI_SCREEN_HEIGHT - TABOS_GUI_DOCK_HEIGHT};

void desktop_model_damage(desktop_model_t* model, tabos_gui_rect_t rectangle)
{
    tabos_gui_rect_t clipped;
    if (!tabos_gui_intersect(rectangle, screen, &clipped)) {
        return;
    }
    if (!model->damaged) {
        model->damage  = clipped;
        model->damaged = true;
        return;
    }
    const int32_t left   = clipped.x < model->damage.x ? clipped.x : model->damage.x;
    const int32_t top    = clipped.y < model->damage.y ? clipped.y : model->damage.y;
    const int32_t right  = clipped.x + clipped.width > model->damage.x + model->damage.width ?
                               clipped.x + clipped.width :
                               model->damage.x + model->damage.width;
    const int32_t bottom = clipped.y + clipped.height > model->damage.y + model->damage.height ?
                               clipped.y + clipped.height :
                               model->damage.y + model->damage.height;
    model->damage        = (tabos_gui_rect_t) {left, top, right - left, bottom - top};
}

void desktop_model_init(desktop_model_t* model)
{
    *model = (desktop_model_t) {.focus = -1, .drag_window = -1};
    desktop_model_damage(model, screen);
}

int desktop_model_add(desktop_model_t* model, int pid, const char* title)
{
    if (pid <= 0 || title == NULL || model->count >= TABOS_GUI_WINDOW_MAX) {
        return -1;
    }
    for (unsigned int index = 0U; index < TABOS_GUI_WINDOW_MAX; ++index) {
        if (model->windows[index].occupied && model->windows[index].pid == pid) {
            return -1;
        }
    }
    for (unsigned int index = 0U; index < TABOS_GUI_WINDOW_MAX; ++index) {
        desktop_window_t* window = &model->windows[index];
        if (!window->occupied) {
            *window = (desktop_window_t) {
                .occupied  = true,
                .maximized = true,
                .pid       = pid,
                .channel   = -1,
                .surface   = -1,
                .bounds    = maximized_bounds,
                .restored  = {160, 64, 960, 528}
            };
            size_t length = strlen(title);
            if (length >= sizeof(window->title)) {
                length = sizeof(window->title) - 1U;
            }
            memcpy(window->title, title, length);
            model->stack[model->count++] = index;
            desktop_model_focus(model, index);
            return (int) index;
        }
    }
    return -1;
}

void desktop_model_focus(desktop_model_t* model, unsigned int slot)
{
    if (slot >= TABOS_GUI_WINDOW_MAX || !model->windows[slot].occupied) {
        return;
    }
    if (model->focus >= 0) {
        desktop_model_damage(model, model->windows[model->focus].bounds);
    }
    for (size_t index = 0U; index < model->count; ++index) {
        if (model->stack[index] == slot) {
            memmove(model->stack + index, model->stack + index + 1U,
                    (model->count - index - 1U) * sizeof(model->stack[0]));
            model->stack[model->count - 1U] = slot;
            break;
        }
    }
    model->focus                   = (int) slot;
    model->windows[slot].minimized = false;
    desktop_model_damage(model, model->windows[slot].bounds);
    desktop_model_damage(model, (tabos_gui_rect_t) {0, 640, 1280, 80});
}

static void choose_focus(desktop_model_t* model)
{
    model->focus = -1;
    for (size_t index = model->count; index > 0U; --index) {
        const unsigned int slot = model->stack[index - 1U];
        if (!model->windows[slot].minimized) {
            model->focus = (int) slot;
            desktop_model_damage(model, model->windows[slot].bounds);
            break;
        }
    }
}

void desktop_model_remove(desktop_model_t* model, unsigned int slot)
{
    if (slot >= TABOS_GUI_WINDOW_MAX || !model->windows[slot].occupied) {
        return;
    }
    desktop_model_damage(model, model->windows[slot].bounds);
    for (size_t index = 0U; index < model->count; ++index) {
        if (model->stack[index] == slot) {
            memmove(model->stack + index, model->stack + index + 1U,
                    (model->count - index - 1U) * sizeof(model->stack[0]));
            --model->count;
            break;
        }
    }
    model->windows[slot] = (desktop_window_t) {0};
    if (model->drag_window == (int) slot) {
        (void) desktop_model_drag_end(model, true);
    }
    choose_focus(model);
    desktop_model_damage(model, (tabos_gui_rect_t) {0, 640, 1280, 80});
}

void desktop_model_minimize(desktop_model_t* model, unsigned int slot)
{
    if (slot >= TABOS_GUI_WINDOW_MAX || !model->windows[slot].occupied) {
        return;
    }
    model->windows[slot].minimized = true;
    desktop_model_damage(model, model->windows[slot].bounds);
    choose_focus(model);
    desktop_model_damage(model, (tabos_gui_rect_t) {0, 640, 1280, 80});
}

int desktop_model_hit(const desktop_model_t* model, int32_t x, int32_t y)
{
    if (y >= 640) {
        return -1;
    }
    for (size_t index = model->count; index > 0U; --index) {
        const unsigned int slot = model->stack[index - 1U];
        if (!model->windows[slot].minimized && tabos_gui_contains(model->windows[slot].bounds, x, y)) {
            return (int) slot;
        }
    }
    return -1;
}

bool desktop_model_configure(desktop_model_t* model, unsigned int slot, tabos_gui_rect_t bounds, bool maximized)
{
    if (slot >= TABOS_GUI_WINDOW_MAX || !model->windows[slot].occupied) {
        return false;
    }
    desktop_window_t* window = &model->windows[slot];
    if (window->pending || window->requested_serial == UINT32_MAX || bounds.width < 320 || bounds.height < 240 ||
        bounds.width > 1280 || bounds.height > 640 || bounds.x < 0 || bounds.y < 0 || bounds.x > 1280 - bounds.width ||
        bounds.y > 640 - bounds.height) {
        return false;
    }
    window->proposed          = bounds;
    window->pending           = true;
    window->pending_maximized = maximized;
    ++window->requested_serial;
    return true;
}

bool desktop_model_adopt(desktop_model_t* model, unsigned int slot, uint32_t serial, tabos_surface_t surface,
                         uint32_t width, uint32_t height)
{
    if (slot >= TABOS_GUI_WINDOW_MAX || !model->windows[slot].occupied || surface <= 0) {
        return false;
    }
    desktop_window_t* window = &model->windows[slot];
    const tabos_gui_rect_t bounds =
        window->pending && serial == window->requested_serial ? window->proposed : window->bounds;
    if ((serial != window->serial && (!window->pending || serial != window->requested_serial)) ||
        width != (uint32_t) bounds.width || height != (uint32_t) bounds.height - TABOS_GUI_TITLE_HEIGHT) {
        return false;
    }
    desktop_model_damage(model, window->bounds);
    if (window->pending && serial == window->requested_serial) {
        if (!window->maximized) {
            window->restored = window->bounds;
        }
        window->bounds    = window->proposed;
        window->maximized = window->pending_maximized;
        window->serial    = serial;
        window->pending   = false;
        if (!window->maximized) {
            window->restored = window->bounds;
        }
    }
    window->surface = surface;
    desktop_model_damage(model, window->bounds);
    return true;
}

void desktop_model_resize_failed(desktop_model_t* model, unsigned int slot, uint32_t serial)
{
    if (slot < TABOS_GUI_WINDOW_MAX && model->windows[slot].occupied &&
        model->windows[slot].requested_serial == serial) {
        model->windows[slot].pending = false;
        desktop_model_damage(model, model->windows[slot].bounds);
    }
}

void desktop_model_drag_begin(desktop_model_t* model, unsigned int slot, int32_t x, int32_t y, bool resize)
{
    if (slot >= TABOS_GUI_WINDOW_MAX || !model->windows[slot].occupied || model->windows[slot].maximized ||
        model->windows[slot].pending) {
        return;
    }
    model->dragging    = true;
    model->resizing    = resize;
    model->drag_window = (int) slot;
    model->drag_x      = x;
    model->drag_y      = y;
    model->drag_origin = model->windows[slot].bounds;
    model->outline     = model->drag_origin;
}

static int32_t clamp(int64_t value, int32_t low, int32_t high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return (int32_t) value;
}

void desktop_model_drag_move(desktop_model_t* model, int32_t x, int32_t y)
{
    if (!model->dragging || model->drag_window < 0) {
        return;
    }
    desktop_window_t* window = &model->windows[model->drag_window];
    desktop_model_damage(model, model->resizing ? model->outline : window->bounds);
    const int64_t dx = (int64_t) x - model->drag_x;
    const int64_t dy = (int64_t) y - model->drag_y;
    if (model->resizing) {
        model->outline.width  = clamp((int64_t) model->drag_origin.width + dx, 320, 1280 - model->outline.x);
        model->outline.height = clamp((int64_t) model->drag_origin.height + dy, 240, 640 - model->outline.y);
        desktop_model_damage(model, model->outline);
    } else {
        window->bounds.x = clamp((int64_t) model->drag_origin.x + dx, 0, 1280 - window->bounds.width);
        window->bounds.y = clamp((int64_t) model->drag_origin.y + dy, 0, 640 - window->bounds.height);
        desktop_model_damage(model, window->bounds);
    }
}

bool desktop_model_drag_end(desktop_model_t* model, bool cancel)
{
    if (!model->dragging || model->drag_window < 0) {
        return false;
    }
    const unsigned int slot  = (unsigned int) model->drag_window;
    desktop_window_t* window = &model->windows[slot];
    desktop_model_damage(model, model->outline);
    model->dragging    = false;
    model->drag_window = -1;
    if (!window->occupied) {
        return false;
    }
    if (cancel) {
        desktop_model_damage(model, window->bounds);
        window->bounds = model->drag_origin;
        desktop_model_damage(model, window->bounds);
        return false;
    }
    if (model->resizing) {
        return desktop_model_configure(model, slot, model->outline, false);
    }
    window->restored = window->bounds;
    return false;
}
