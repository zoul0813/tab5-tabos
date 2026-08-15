#ifndef TABOS_INTERNAL_CONSOLE_H
#define TABOS_INTERNAL_CONSOLE_H

#include <tabos/internal/terminal.h>

void console_init(terminal_t *terminal);
void console_rebind(terminal_t *terminal);
void console_shutdown(void);
void console_update(void);

#endif
