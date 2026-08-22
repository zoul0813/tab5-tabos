#ifndef TABOS_INTERNAL_TERMINAL_H
#define TABOS_INTERNAL_TERMINAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <tabos/platform/platform.h>

typedef struct {
    char character;
    platform_pixel_t foreground;
    platform_pixel_t background;
} terminal_cell_t;

typedef struct {
    platform_framebuffer_t *framebuffer;
    size_t column;
    size_t row;
    size_t columns;
    size_t rows;
    unsigned int scale;
    platform_pixel_t foreground;
    platform_pixel_t background;
    terminal_cell_t *cells;
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
    uint8_t ansi_state;
    unsigned int ansi_value;
    bool ansi_have_value;
    size_t saved_column;
    uint64_t saved_line;
} terminal_t;

bool terminal_init(terminal_t *terminal, platform_framebuffer_t *framebuffer, unsigned int scale);
bool terminal_resize(terminal_t *terminal, platform_framebuffer_t *framebuffer,
                         unsigned int scale);
void terminal_shutdown(terminal_t *terminal);
void terminal_clear(terminal_t *terminal);
void terminal_redraw(terminal_t *terminal);
void terminal_set_colors(terminal_t *terminal, platform_pixel_t foreground,
                             platform_pixel_t background);
void terminal_write(terminal_t *terminal, const char *text);
void terminal_write_line(terminal_t *terminal, const char *text);
void terminal_set_cursor_visible(terminal_t *terminal, bool visible);
void terminal_set_cursor_phase(terminal_t *terminal, bool visible);
bool terminal_scroll_lines(terminal_t *terminal, int lines);
bool terminal_page_up(terminal_t *terminal);
bool terminal_page_down(terminal_t *terminal);
bool terminal_scroll_to_start(terminal_t *terminal);
bool terminal_scroll_to_end(terminal_t *terminal);
bool terminal_is_at_end(const terminal_t *terminal);
size_t terminal_history_line_count(const terminal_t *terminal);

#endif
