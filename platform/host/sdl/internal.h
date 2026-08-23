#ifndef TABOS_HOST_SDL_INTERNAL_H
#define TABOS_HOST_SDL_INTERNAL_H

#include <stdbool.h>

#include <SDL3/SDL.h>

extern SDL_Window* host_window;

bool host_is_headless(void);
void host_request_quit(void);
void host_input_update(bool wait);
bool host_capture_screenshot(void);

#endif
