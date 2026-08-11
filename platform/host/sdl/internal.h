#ifndef TABOS_HOST_SDL_INTERNAL_H
#define TABOS_HOST_SDL_INTERNAL_H

#include <stdbool.h>

#include <SDL3/SDL.h>

extern SDL_Window *tab_host_window;

bool tab_host_is_headless(void);
void tab_host_request_quit(void);
void tab_host_input_update(bool wait);

#endif
