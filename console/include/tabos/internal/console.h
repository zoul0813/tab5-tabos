#ifndef TABOS_INTERNAL_CONSOLE_H
#define TABOS_INTERNAL_CONSOLE_H

#include <stdint.h>
#include <tabos/console.h>
#include <tabos/tty.h>

#include <tabos/internal/terminal.h>

bool console_get_size(const tabos_console_session_t* session, tabos_tty_size_t* size);
bool console_init(terminal_t* terminal);
void console_rebind(terminal_t* terminal);
bool console_write_panic(const char* text);
void console_shutdown(void);
void console_update(void);
uint64_t console_next_deadline(void);
void console_redraw(void);
void console_set_graphics_active(bool active);
bool console_graphics_active(void);

#endif
