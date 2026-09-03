#include "internal.h"

#include <tabos/config/display.h>
#include <tabos/internal/pointer.h>
#include <tabos/platform/platform.h>

#include <string.h>

typedef struct {
        SDL_FingerID finger;
        bool active;
} host_touch_contact_t;

static host_touch_contact_t touch_contacts[TABOS_POINTER_MAX_CONTACTS];
static uint32_t mouse_buttons;
static int32_t mouse_x;
static int32_t mouse_y;
static bool mouse_active;
static bool pointer_ready;

static uint32_t pointer_buttons(Uint32 buttons)
{
    uint32_t result = 0U;
    if ((buttons & SDL_BUTTON_LMASK) != 0U) {
        result |= TABOS_POINTER_BUTTON_PRIMARY;
    }
    if ((buttons & SDL_BUTTON_RMASK) != 0U) {
        result |= TABOS_POINTER_BUTTON_SECONDARY;
    }
    if ((buttons & SDL_BUTTON_MMASK) != 0U) {
        result |= TABOS_POINTER_BUTTON_MIDDLE;
    }
    return result;
}

static bool logical_coordinates(float window_x, float window_y, int32_t* x, int32_t* y)
{
    int window_width  = 0;
    int window_height = 0;
    if (host_window == NULL) {
        *x = (int32_t) window_x;
        *y = (int32_t) window_y;
        return *x >= 0 && *y >= 0 && *x < TABOS_DISPLAY_WIDTH && *y < TABOS_DISPLAY_HEIGHT;
    }
    if (!SDL_GetWindowSizeInPixels(host_window, &window_width, &window_height) || window_width <= 0 ||
        window_height <= 0) {
        return false;
    }
    const float scale_x     = (float) window_width / (float) TABOS_DISPLAY_WIDTH;
    const float scale_y     = (float) window_height / (float) TABOS_DISPLAY_HEIGHT;
    const float scale       = scale_x < scale_y ? scale_x : scale_y;
    const float offset_x    = ((float) window_width - ((float) TABOS_DISPLAY_WIDTH * scale)) * 0.5F;
    const float offset_y    = ((float) window_height - ((float) TABOS_DISPLAY_HEIGHT * scale)) * 0.5F;
    const int32_t logical_x = (int32_t) ((window_x - offset_x) / scale);
    const int32_t logical_y = (int32_t) ((window_y - offset_y) / scale);
    if (logical_x < 0 || logical_y < 0 || logical_x >= TABOS_DISPLAY_WIDTH || logical_y >= TABOS_DISPLAY_HEIGHT) {
        return false;
    }
    *x = logical_x;
    *y = logical_y;
    return true;
}

static int contact_for_finger(SDL_FingerID finger, bool create)
{
    for (uint32_t contact = 1U; contact < TABOS_POINTER_MAX_CONTACTS; ++contact) {
        if (touch_contacts[contact].active && touch_contacts[contact].finger == finger) {
            return (int) contact;
        }
    }
    if (!create) {
        return -1;
    }
    for (uint32_t contact = 1U; contact < TABOS_POINTER_MAX_CONTACTS; ++contact) {
        if (!touch_contacts[contact].active) {
            touch_contacts[contact] = (host_touch_contact_t) {.finger = finger, .active = true};
            return (int) contact;
        }
    }
    return -1;
}

static bool mouse_event(const SDL_Event* event)
{
    if (event->type != SDL_EVENT_MOUSE_MOTION && event->type != SDL_EVENT_MOUSE_BUTTON_DOWN &&
        event->type != SDL_EVENT_MOUSE_BUTTON_UP) {
        return false;
    }
    const SDL_MouseID source = event->type == SDL_EVENT_MOUSE_MOTION ? event->motion.which : event->button.which;
    if (source == SDL_TOUCH_MOUSEID) {
        return true;
    }
    const float window_x = event->type == SDL_EVENT_MOUSE_MOTION ? event->motion.x : event->button.x;
    const float window_y = event->type == SDL_EVENT_MOUSE_MOTION ? event->motion.y : event->button.y;
    int32_t x            = 0;
    int32_t y            = 0;
    if (!logical_coordinates(window_x, window_y, &x, &y)) {
        if (mouse_active) {
            const tabos_pointer_event_t cancel = {
                .type       = TABOS_POINTER_CANCEL,
                .contact_id = 0U,
                .x          = mouse_x,
                .y          = mouse_y,
            };
            pointer_service_submit(&cancel);
            mouse_active  = false;
            mouse_buttons = 0U;
        }
        return true;
    }
    const uint32_t previous_buttons = mouse_buttons;
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        mouse_buttons = pointer_buttons(event->motion.state);
    } else {
        const uint32_t button = pointer_buttons(SDL_BUTTON_MASK(event->button.button));
        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            mouse_buttons |= button;
        } else {
            mouse_buttons &= ~button;
        }
    }
    tabos_pointer_event_type_t type = TABOS_POINTER_MOVE;
    if (!mouse_active && mouse_buttons != 0U) {
        type = TABOS_POINTER_DOWN;
    } else if (mouse_active && mouse_buttons == 0U) {
        type = TABOS_POINTER_UP;
    }
    mouse_active = mouse_buttons != 0U;
    mouse_x      = x;
    mouse_y      = y;
    if (type == TABOS_POINTER_MOVE && !mouse_active && previous_buttons == 0U) {
        return true;
    }
    const tabos_pointer_event_t pointer = {
        .type       = type,
        .contact_id = 0U,
        .x          = x,
        .y          = y,
        .buttons    = mouse_buttons,
    };
    pointer_service_submit(&pointer);
    return true;
}

static bool touch_event(const SDL_Event* event)
{
    if (event->type != SDL_EVENT_FINGER_DOWN && event->type != SDL_EVENT_FINGER_MOTION &&
        event->type != SDL_EVENT_FINGER_UP && event->type != SDL_EVENT_FINGER_CANCELED) {
        return false;
    }
    const bool create = event->type == SDL_EVENT_FINGER_DOWN;
    const int contact = contact_for_finger(event->tfinger.fingerID, create);
    if (contact < 0) {
        return true;
    }
    tabos_pointer_event_type_t type = TABOS_POINTER_MOVE;
    if (event->type == SDL_EVENT_FINGER_DOWN) {
        type = TABOS_POINTER_DOWN;
    } else if (event->type == SDL_EVENT_FINGER_UP) {
        type = TABOS_POINTER_UP;
    } else if (event->type == SDL_EVENT_FINGER_CANCELED) {
        type = TABOS_POINTER_CANCEL;
    }
    int32_t x = (int32_t) (event->tfinger.x * (float) TABOS_DISPLAY_WIDTH);
    int32_t y = (int32_t) (event->tfinger.y * (float) TABOS_DISPLAY_HEIGHT);
    if (x >= TABOS_DISPLAY_WIDTH) {
        x = TABOS_DISPLAY_WIDTH - 1;
    }
    if (y >= TABOS_DISPLAY_HEIGHT) {
        y = TABOS_DISPLAY_HEIGHT - 1;
    }
    const float pressure                = event->tfinger.pressure;
    const tabos_pointer_event_t pointer = {
        .type       = type,
        .contact_id = (uint32_t) contact,
        .x          = x,
        .y          = y,
        .buttons    = type == TABOS_POINTER_UP || type == TABOS_POINTER_CANCEL ? 0U : TABOS_POINTER_BUTTON_PRIMARY,
        .pressure   = pressure > 0.0F ? (uint32_t) (pressure * (float) TABOS_POINTER_PRESSURE_MAX) : 0U,
        .flags      = pressure > 0.0F ? TABOS_POINTER_EVENT_HAS_PRESSURE : 0U,
    };
    pointer_service_submit(&pointer);
    if (type == TABOS_POINTER_UP || type == TABOS_POINTER_CANCEL) {
        touch_contacts[contact] = (host_touch_contact_t) {0};
    }
    return true;
}

bool platform_pointer_init(const char** driver, int* error)
{
    memset(touch_contacts, 0, sizeof(touch_contacts));
    mouse_buttons = 0U;
    mouse_active  = false;
    pointer_ready = true;
    if (driver != NULL) {
        *driver = "SDL3 pointer";
    }
    if (error != NULL) {
        *error = 0;
    }
    return true;
}

void platform_pointer_update(void)
{
}

void platform_pointer_shutdown(void)
{
    pointer_ready = false;
    memset(touch_contacts, 0, sizeof(touch_contacts));
    mouse_buttons = 0U;
    mouse_active  = false;
}

bool host_pointer_event(const SDL_Event* event)
{
    if (!pointer_ready || event == NULL) {
        return false;
    }
    return mouse_event(event) || touch_event(event);
}
