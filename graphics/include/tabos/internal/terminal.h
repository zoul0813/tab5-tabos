#ifndef TABOS_INTERNAL_TERMINAL_H
#define TABOS_INTERNAL_TERMINAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <tabos/platform/platform.h>

typedef struct {
    char character;
    tab_pixel_t foreground;
    tab_pixel_t background;
} tab_terminal_cell_t;

typedef struct {
    tab_framebuffer_t *framebuffer;
    size_t column;
    size_t row;
    size_t columns;
    size_t rows;
    unsigned int scale;
    tab_pixel_t foreground;
    tab_pixel_t background;
    tab_terminal_cell_t *cells;
    bool *dirty_cells;
    size_t *line_lengths;
    bool *hard_breaks;
    size_t line_capacity;
    uint64_t first_line;
    uint64_t current_line;
    uint64_t viewport_top;
    bool cursor_visible;
    bool cursor_phase_visible;
    bool full_redraw;
} tab_terminal_t;

bool tab_terminal_init(tab_terminal_t *terminal, tab_framebuffer_t *framebuffer, unsigned int scale);
bool tab_terminal_resize(tab_terminal_t *terminal, tab_framebuffer_t *framebuffer,
                         unsigned int scale);
void tab_terminal_shutdown(tab_terminal_t *terminal);
void tab_terminal_clear(tab_terminal_t *terminal);
void tab_terminal_set_colors(tab_terminal_t *terminal, tab_pixel_t foreground,
                             tab_pixel_t background);
void tab_terminal_write(tab_terminal_t *terminal, const char *text);
void tab_terminal_write_line(tab_terminal_t *terminal, const char *text);
void tab_terminal_set_cursor_visible(tab_terminal_t *terminal, bool visible);
void tab_terminal_set_cursor_phase(tab_terminal_t *terminal, bool visible);
bool tab_terminal_scroll_lines(tab_terminal_t *terminal, int lines);
bool tab_terminal_page_up(tab_terminal_t *terminal);
bool tab_terminal_page_down(tab_terminal_t *terminal);
bool tab_terminal_scroll_to_start(tab_terminal_t *terminal);
bool tab_terminal_scroll_to_end(tab_terminal_t *terminal);
bool tab_terminal_is_at_end(const tab_terminal_t *terminal);
size_t tab_terminal_history_line_count(const tab_terminal_t *terminal);

#endif
