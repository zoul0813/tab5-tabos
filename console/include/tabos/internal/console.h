#ifndef TABOS_INTERNAL_CONSOLE_H
#define TABOS_INTERNAL_CONSOLE_H

#include <tabos/internal/terminal.h>

bool console_init(terminal_t *terminal);
void console_rebind(terminal_t *terminal);
bool console_write_panic(const char *text);
void console_shutdown(void);
void console_update(void);
void console_redraw(void);
void console_set_graphics_active(bool active);

#endif
