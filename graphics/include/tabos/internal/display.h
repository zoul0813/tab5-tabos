#ifndef TABOS_INTERNAL_DISPLAY_H
#define TABOS_INTERNAL_DISPLAY_H

#include <stdbool.h>

#include <tabos/platform/platform.h>

bool tab_display_init(void);
void tab_display_render_diagnostic(void);
bool tab_display_present(void);
const tab_framebuffer_t *tab_display_framebuffer(void);
void tab_display_shutdown(void);

#endif
