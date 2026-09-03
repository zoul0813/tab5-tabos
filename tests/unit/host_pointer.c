#include "internal.h"

#include <tabos/internal/pointer.h>

#include <stdio.h>

static int failures;

static void expect(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "host pointer test failed: %s\n", message);
        ++failures;
    }
}

int main(void)
{
    static const int owner;
    expect(pointer_service_init(), "service initializes");
    pointer_service_set_device_id(9U);
    pointer_service_set_foreground_owner(&owner);
    const tabos_pointer_stream_t stream = pointer_service_open(&owner, 9U);

    SDL_Event mouse_down     = {.type = SDL_EVENT_MOUSE_BUTTON_DOWN};
    mouse_down.button.which  = 1U;
    mouse_down.button.button = SDL_BUTTON_LEFT;
    mouse_down.button.x      = 120.0F;
    mouse_down.button.y      = 240.0F;
    expect(host_pointer_event(&mouse_down), "mouse down consumed");
    tabos_pointer_event_t event;
    expect(pointer_service_read(&owner, stream, &event) == 0 && event.type == TABOS_POINTER_DOWN &&
               event.contact_id == 0U && event.x == 120 && event.y == 240,
           "mouse maps to contact zero");

    SDL_Event finger_down        = {.type = SDL_EVENT_FINGER_DOWN};
    finger_down.tfinger.fingerID = 77U;
    finger_down.tfinger.x        = 0.25F;
    finger_down.tfinger.y        = 0.5F;
    finger_down.tfinger.pressure = 0.5F;
    expect(host_pointer_event(&finger_down), "finger down consumed");
    expect(pointer_service_read(&owner, stream, &event) == 0 && event.type == TABOS_POINTER_DOWN &&
               event.contact_id == 1U && event.x == 320 && event.y == 360 &&
               (event.flags & TABOS_POINTER_EVENT_HAS_PRESSURE) != 0U,
           "touch maps normalized coordinates and pressure");

    SDL_Event finger_motion = finger_down;
    finger_motion.type      = SDL_EVENT_FINGER_MOTION;
    finger_motion.tfinger.x = 0.5F;
    expect(host_pointer_event(&finger_motion), "finger motion consumed");
    expect(pointer_service_read(&owner, stream, &event) == 0 && event.type == TABOS_POINTER_MOVE &&
               event.contact_id == 1U && event.x == 640,
           "touch contact ID remains stable");

    SDL_Event finger_up = finger_motion;
    finger_up.type      = SDL_EVENT_FINGER_UP;
    expect(host_pointer_event(&finger_up), "finger up consumed");
    expect(pointer_service_read(&owner, stream, &event) == 0 && event.type == TABOS_POINTER_UP &&
               event.contact_id == 1U,
           "touch up retains contact ID");

    (void) pointer_service_close(&owner, stream);
    pointer_service_shutdown();
    return failures == 0 ? 0 : 1;
}
