#ifndef TABOS_INTERNAL_CONSOLE_H
#define TABOS_INTERNAL_CONSOLE_H

#include <stdint.h>
#include <tabos/console.h>
#include <tabos/tty.h>

#include <tabos/internal/terminal.h>

typedef enum {
    CONSOLE_RESIZE_FAILED,
    CONSOLE_RESIZE_PRESENT_FAILED,
    CONSOLE_RESIZE_OK,
} console_resize_result_t;

bool console_get_size(const tabos_console_session_t* session, tabos_tty_size_t* size);
bool console_init(terminal_t* terminal);
console_resize_result_t console_resize(platform_framebuffer_t* framebuffer, unsigned int scale);
bool console_write_panic(const char* text);
void console_shutdown(void);
void console_update(void);
uint64_t console_next_deadline(void);
void console_redraw(void);
void console_set_graphics_active(bool active);
bool console_graphics_active(void);

#endif
