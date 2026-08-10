#ifndef TABOS_TERMINAL_H
#define TABOS_TERMINAL_H

#include <stdbool.h>

enum {
    TABOS_TERMINAL_SCALE_MIN = 1,
    TABOS_TERMINAL_SCALE_MAX = 8,
};

/*
 * Set terminal glyph scale. The setting can be changed before or after runtime
 * startup. An active boot console is immediately reflowed and redrawn.
 */
bool tabos_terminal_set_scale(unsigned int scale);
unsigned int tabos_terminal_get_scale(void);

#endif
