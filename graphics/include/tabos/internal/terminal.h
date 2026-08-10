#ifndef TABOS_INTERNAL_TERMINAL_H
#define TABOS_INTERNAL_TERMINAL_H

#include <stdbool.h>
#include <stddef.h>

#include <tabos/platform/platform.h>

typedef struct {
    tab_framebuffer_t *framebuffer;
    size_t column;
    size_t row;
    size_t columns;
    size_t rows;
    unsigned int scale;
    tab_pixel_t foreground;
    tab_pixel_t background;
} tab_terminal_t;

bool tab_terminal_init(tab_terminal_t *terminal, tab_framebuffer_t *framebuffer, unsigned int scale);
void tab_terminal_clear(tab_terminal_t *terminal);
void tab_terminal_set_colors(tab_terminal_t *terminal, tab_pixel_t foreground,
                             tab_pixel_t background);
void tab_terminal_write(tab_terminal_t *terminal, const char *text);
void tab_terminal_write_line(tab_terminal_t *terminal, const char *text);

#endif
