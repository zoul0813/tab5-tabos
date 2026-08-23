#ifndef TABOS_INTERNAL_DISPLAY_H
#define TABOS_INTERNAL_DISPLAY_H

#include <stdbool.h>

#include <tabos/platform/platform.h>

bool display_init(void);
bool display_present(void);
platform_framebuffer_t* display_framebuffer(void);
void display_shutdown(void);

#endif
