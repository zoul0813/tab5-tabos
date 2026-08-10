#ifndef TABOS_INTERNAL_CONSOLE_H
#define TABOS_INTERNAL_CONSOLE_H

#include <tabos/internal/terminal.h>

void tab_console_init(tab_terminal_t *terminal);
void tab_console_rebind(tab_terminal_t *terminal);
void tab_console_shutdown(void);

#endif
