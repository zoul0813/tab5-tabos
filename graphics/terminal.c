#include <tabos/internal/terminal.h>

#include <tabos/internal/font.h>
#include <tabos/internal/raster.h>

#include <tabos/config/display.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t cell_width(const terminal_t* terminal)
{
    return TABOS_FONT_CELL_WIDTH * terminal->scale;
}

static size_t cell_height(const terminal_t* terminal)
{
    return TABOS_FONT_CELL_HEIGHT * terminal->scale;
}

static size_t line_slot(const terminal_t* terminal, uint64_t line)
{
    return (size_t) (line % terminal->line_capacity);
}

static terminal_cell_t* line_cells(terminal_t* terminal, uint64_t line)
{
    return terminal->cells + (line_slot(terminal, line) * terminal->columns);
}

static const terminal_cell_t* const_line_cells(const terminal_t* terminal, uint64_t line)
{
    return terminal->cells + (line_slot(terminal, line) * terminal->columns);
}

static uint64_t live_viewport_top(const terminal_t* terminal)
{
    const uint64_t visible_rows = terminal->rows;
    const uint64_t first_visible =
        terminal->current_line + 1U > visible_rows ? terminal->current_line + 1U - visible_rows : 0U;
    return first_visible > terminal->first_line ? first_visible : terminal->first_line;
}

static void update_cursor_row(terminal_t* terminal)
{
    terminal->row = (size_t) (terminal->current_line - live_viewport_top(terminal));
}

static void follow_live_output(terminal_t* terminal)
{
    const uint64_t viewport_top = live_viewport_top(terminal);
    if (terminal->viewport_top != viewport_top) {
        terminal->viewport_top = viewport_top;
        terminal->full_redraw  = true;
    }
    update_cursor_row(terminal);
}

static void mark_cell(terminal_t* terminal, uint64_t line, size_t column)
{
    if (column >= terminal->columns || line < terminal->viewport_top ||
        line >= terminal->viewport_top + terminal->rows) {
        return;
    }
    const size_t row                                          = (size_t) (line - terminal->viewport_top);
    terminal->dirty_cells[(row * terminal->columns) + column] = true;
}

static bool cursor_should_draw(const terminal_t* terminal)
{
    return terminal->cursor_visible && terminal->cursor_phase_visible &&
           terminal->viewport_top == live_viewport_top(terminal) && terminal->current_line >= terminal->viewport_top &&
           terminal->current_line < terminal->viewport_top + terminal->rows;
}

static void mark_cursor(terminal_t* terminal)
{
    mark_cell(terminal, terminal->current_line, terminal->column);
}

static void clear_line(terminal_t* terminal, uint64_t line)
{
    const size_t slot = line_slot(terminal, line);
    memset(terminal->cells + (slot * terminal->columns), 0, terminal->columns * sizeof(*terminal->cells));
    terminal->line_lengths[slot] = 0U;
    terminal->hard_breaks[slot]  = false;
}

static void append_line(terminal_t* terminal, bool hard_break)
{
    terminal->hard_breaks[line_slot(terminal, terminal->current_line)] = hard_break;
    ++terminal->current_line;
    if (terminal->current_line - terminal->first_line + 1U > terminal->line_capacity) {
        ++terminal->first_line;
    }
    clear_line(terminal, terminal->current_line);
    terminal->column = 0U;
    follow_live_output(terminal);
}

static void put_character(terminal_t* terminal, char character)
{
    terminal_cell_t* cells  = line_cells(terminal, terminal->current_line);
    cells[terminal->column] = (terminal_cell_t) {
        .character  = character,
        .foreground = terminal->foreground,
        .background = terminal->background,
    };
    mark_cell(terminal, terminal->current_line, terminal->column);
    const size_t slot = line_slot(terminal, terminal->current_line);
    if (terminal->line_lengths[slot] < terminal->column + 1U) {
        terminal->line_lengths[slot] = terminal->column + 1U;
    }
    ++terminal->column;
    if (terminal->column >= terminal->columns) {
        append_line(terminal, false);
    }
}

static void trim_line(terminal_t* terminal, uint64_t line)
{
    const size_t slot      = line_slot(terminal, line);
    terminal_cell_t* cells = line_cells(terminal, line);
    while (terminal->line_lengths[slot] > 0U && cells[terminal->line_lengths[slot] - 1U].character == '\0') {
        --terminal->line_lengths[slot];
    }
}

static void backspace(terminal_t* terminal)
{
    if (terminal->column == 0U) {
        if (terminal->current_line == terminal->first_line) {
            return;
        }
        clear_line(terminal, terminal->current_line);
        --terminal->current_line;
        const size_t previous_length = terminal->line_lengths[line_slot(terminal, terminal->current_line)];
        terminal->column             = previous_length > terminal->columns ? terminal->columns : previous_length;
        terminal->hard_breaks[line_slot(terminal, terminal->current_line)] = false;
    }
    if (terminal->column > 0U) {
        --terminal->column;
        line_cells(terminal, terminal->current_line)[terminal->column] = (terminal_cell_t) {0};
        mark_cell(terminal, terminal->current_line, terminal->column);
        trim_line(terminal, terminal->current_line);
    }
    follow_live_output(terminal);
}

static void tab(terminal_t* terminal)
{
    enum {
        TEST_WIDTH = 4
    };
    const size_t spaces = TEST_WIDTH - (terminal->column % TEST_WIDTH);
    for (size_t index = 0U; index < spaces; ++index) {
        put_character(terminal, ' ');
    }
}

static platform_pixel_t ansi_color(unsigned int value)
{
    static const platform_pixel_t colors[] = {
        0x0000, 0xf800, 0x07e0, 0xffe0, 0x001f, 0xf81f, 0x07ff, 0xffff,
    };
    return colors[value < 8U ? value : 7U];
}

static void ansi_sgr(terminal_t* terminal, unsigned int value)
{
    if (value == 0U) {
        terminal->foreground = 0xffff;
        terminal->background = 0x0000;
    } else if (value >= 30U && value <= 37U) {
        terminal->foreground = ansi_color(value - 30U);
    } else if (value >= 40U && value <= 47U) {
        terminal->background = ansi_color(value - 40U);
    }
}

static void ansi_command(terminal_t* terminal, char command)
{
    const unsigned int value = terminal->ansi_have_value ? terminal->ansi_value : 0U;
    if (command == 'H' || command == 'f') {
        terminal->current_line = terminal->first_line + (value > 0U ? value - 1U : 0U);
        terminal->column       = 0U;
    } else if (command == 'A') {
        terminal->current_line -= value < terminal->current_line ? value : terminal->current_line;
    } else if (command == 'B') {
        terminal->current_line += value;
    } else if (command == 'C') {
        terminal->column += value;
        if (terminal->column >= terminal->columns) {
            terminal->column = terminal->columns - 1U;
        }
    } else if (command == 'D') {
        terminal->column -= value < terminal->column ? value : terminal->column;
    } else if (command == 'G') {
        terminal->column = value > 0U ? value - 1U : 0U;
        if (terminal->column >= terminal->columns) {
            terminal->column = terminal->columns - 1U;
        }
    } else if (command == 'J' && value == 2U) {
        terminal_clear(terminal);
    } else if (command == 'K') {
        terminal_cell_t* cells = line_cells(terminal, terminal->current_line);
        const size_t start     = value == 2U ? 0U : terminal->column;
        const size_t end       = value == 2U ? terminal->columns : terminal->columns;
        for (size_t column = start; column < end; ++column) {
            cells[column] = (terminal_cell_t) {0};
        }
        terminal->line_lengths[line_slot(terminal, terminal->current_line)] = start;
        terminal->full_redraw                                               = true;
    } else if (command == 'm') {
        ansi_sgr(terminal, value);
    } else if (command == 's') {
        terminal->saved_column = terminal->column;
        terminal->saved_line   = terminal->current_line;
    } else if (command == 'u') {
        terminal->column       = terminal->saved_column;
        terminal->current_line = terminal->saved_line;
    }
}

static void clear_framebuffer(terminal_t* terminal)
{
    for (size_t y = 0U; y < terminal->framebuffer->height; ++y) {
        for (size_t x = 0U; x < terminal->framebuffer->width; ++x) {
            terminal->framebuffer->pixels[y * terminal->framebuffer->stride_pixels + x] = terminal->background;
        }
    }
}

static void draw_cell(terminal_t* terminal, size_t column, size_t row, const terminal_cell_t* cell, bool cursor)
{
    const char character                       = cell->character == '\0' ? ' ' : cell->character;
    const platform_pixel_t foreground          = cell->character == '\0' ? terminal->foreground : cell->foreground;
    const platform_pixel_t background          = cell->character == '\0' ? terminal->background : cell->background;
    const platform_pixel_t rendered_background = cursor ? foreground : background;
    const size_t width                         = cell_width(terminal);
    const size_t height                        = cell_height(terminal);
    const size_t origin_x                      = column * width;
    const size_t origin_y                      = row * height;
    for (size_t y = 0U; y < height; ++y) {
        raster_fill_span(terminal->framebuffer->pixels + (origin_y + y) * terminal->framebuffer->stride_pixels +
                             origin_x,
                         width, rendered_background);
    }
    (void) font_draw_char(terminal->framebuffer, (int) origin_x, (int) origin_y, character, terminal->scale,
                          cursor ? background : foreground, cursor ? foreground : background);
}

static void render(terminal_t* terminal)
{
    if (terminal == NULL || terminal->framebuffer == NULL || terminal->cells == NULL) {
        return;
    }
    if (!terminal->rendering_enabled) {
        return;
    }
    if (terminal->full_redraw) {
        clear_framebuffer(terminal);
        for (size_t row = 0U; row < terminal->rows; ++row) {
            const uint64_t line = terminal->viewport_top + row;
            if (line < terminal->first_line || line > terminal->current_line) {
                continue;
            }
            const terminal_cell_t* cells = const_line_cells(terminal, line);
            const size_t length          = terminal->line_lengths[line_slot(terminal, line)];
            for (size_t column = 0U; column < length; ++column) {
                draw_cell(terminal, column, row, &cells[column], false);
            }
        }
        memset(terminal->dirty_cells, 0, terminal->rows * terminal->columns * sizeof(*terminal->dirty_cells));
        terminal->full_redraw = false;
    } else {
        for (size_t row = 0U; row < terminal->rows; ++row) {
            const uint64_t line          = terminal->viewport_top + row;
            const terminal_cell_t* cells = line <= terminal->current_line ? const_line_cells(terminal, line) : NULL;
            for (size_t column = 0U; column < terminal->columns; ++column) {
                const size_t dirty_index = (row * terminal->columns) + column;
                if (!terminal->dirty_cells[dirty_index]) {
                    continue;
                }
                const terminal_cell_t empty = {0};
                draw_cell(terminal, column, row, cells == NULL ? &empty : &cells[column], false);
                terminal->dirty_cells[dirty_index] = false;
            }
        }
    }

    if (cursor_should_draw(terminal)) {
        const size_t cursor_row      = (size_t) (terminal->current_line - terminal->viewport_top);
        const terminal_cell_t* cells = const_line_cells(terminal, terminal->current_line);
        draw_cell(terminal, terminal->column, cursor_row, &cells[terminal->column], true);
    }
}

static bool terminal_init_with_rendering(terminal_t* terminal, platform_framebuffer_t* framebuffer, unsigned int scale,
                                         bool rendering_enabled)
{
    if (terminal == NULL || framebuffer == NULL || framebuffer->pixels == NULL || scale == 0U) {
        return false;
    }

    const size_t columns = framebuffer->width / (TABOS_FONT_CELL_WIDTH * scale);
    const size_t rows    = framebuffer->height / (TABOS_FONT_CELL_HEIGHT * scale);
    if (columns == 0U || rows == 0U || rows > SIZE_MAX - TABOS_TERMINAL_SCROLLBACK_LINES) {
        return false;
    }
    const size_t line_capacity = rows + TABOS_TERMINAL_SCROLLBACK_LINES;
    if (columns > SIZE_MAX / line_capacity || columns * line_capacity > SIZE_MAX / sizeof(terminal_cell_t)) {
        return false;
    }

    terminal_cell_t* cells = calloc(columns * line_capacity, sizeof(*cells));
    size_t* line_lengths   = calloc(line_capacity, sizeof(*line_lengths));
    bool* hard_breaks      = calloc(line_capacity, sizeof(*hard_breaks));
    bool* dirty_cells      = calloc(rows * columns, sizeof(*dirty_cells));
    if (cells == NULL || line_lengths == NULL || hard_breaks == NULL || dirty_cells == NULL) {
        free(cells);
        free(line_lengths);
        free(hard_breaks);
        free(dirty_cells);
        return false;
    }

    *terminal = (terminal_t) {
        .framebuffer          = framebuffer,
        .columns              = columns,
        .rows                 = rows,
        .scale                = scale,
        .foreground           = 0xffff,
        .background           = 0x0000,
        .cells                = cells,
        .line_lengths         = line_lengths,
        .hard_breaks          = hard_breaks,
        .dirty_cells          = dirty_cells,
        .line_capacity        = line_capacity,
        .cursor_phase_visible = true,
        .full_redraw          = true,
        .rendering_enabled    = rendering_enabled,
    };
    render(terminal);
    return true;
}

bool terminal_init(terminal_t* terminal, platform_framebuffer_t* framebuffer, unsigned int scale)
{
    return terminal_init_with_rendering(terminal, framebuffer, scale, true);
}

void terminal_shutdown(terminal_t* terminal)
{
    if (terminal == NULL) {
        return;
    }
    free(terminal->cells);
    free(terminal->line_lengths);
    free(terminal->hard_breaks);
    free(terminal->dirty_cells);
    *terminal = (terminal_t) {0};
}

bool terminal_resize(terminal_t* terminal, platform_framebuffer_t* framebuffer, unsigned int scale)
{
    if (terminal == NULL || terminal->cells == NULL) {
        return false;
    }

    terminal_t resized;
    if (!terminal_init_with_rendering(&resized, framebuffer, scale, terminal->rendering_enabled)) {
        return false;
    }
    resized.foreground           = terminal->foreground;
    resized.background           = terminal->background;
    resized.cursor_visible       = terminal->cursor_visible;
    resized.cursor_phase_visible = terminal->cursor_phase_visible;

    const uint64_t old_live_top = live_viewport_top(terminal);
    const uint64_t distance_from_end =
        old_live_top >= terminal->viewport_top ? old_live_top - terminal->viewport_top : 0U;

    for (uint64_t line = terminal->first_line; line <= terminal->current_line; ++line) {
        const size_t slot            = line_slot(terminal, line);
        const terminal_cell_t* cells = const_line_cells(terminal, line);
        for (size_t column = 0U; column < terminal->line_lengths[slot]; ++column) {
            const terminal_cell_t* cell = &cells[column];
            resized.foreground          = cell->foreground;
            resized.background          = cell->background;
            put_character(&resized, cell->character == '\0' ? ' ' : cell->character);
        }
        if (terminal->hard_breaks[slot]) {
            append_line(&resized, true);
        }
    }
    resized.foreground = terminal->foreground;
    resized.background = terminal->background;

    const uint64_t new_live_top = live_viewport_top(&resized);
    const uint64_t available    = new_live_top - resized.first_line;
    resized.viewport_top        = new_live_top - (distance_from_end < available ? distance_from_end : available);
    update_cursor_row(&resized);
    resized.full_redraw = true;
    render(&resized);

    terminal_shutdown(terminal);
    *terminal = resized;
    return true;
}

void terminal_clear(terminal_t* terminal)
{
    if (terminal == NULL || terminal->cells == NULL) {
        return;
    }
    memset(terminal->cells, 0, terminal->columns * terminal->line_capacity * sizeof(*terminal->cells));
    memset(terminal->line_lengths, 0, terminal->line_capacity * sizeof(*terminal->line_lengths));
    memset(terminal->hard_breaks, 0, terminal->line_capacity * sizeof(*terminal->hard_breaks));
    terminal->column               = 0U;
    terminal->row                  = 0U;
    terminal->first_line           = 0U;
    terminal->current_line         = 0U;
    terminal->viewport_top         = 0U;
    terminal->cursor_phase_visible = true;
    terminal->full_redraw          = true;
    render(terminal);
}

void terminal_set_colors(terminal_t* terminal, platform_pixel_t foreground, platform_pixel_t background)
{
    if (terminal != NULL) {
        terminal->foreground = foreground;
        terminal->background = background;
    }
}

void terminal_redraw(terminal_t* terminal)
{
    if (terminal == NULL) {
        return;
    }
    terminal->full_redraw = true;
    render(terminal);
}

void terminal_set_rendering_enabled(terminal_t* terminal, bool enabled)
{
    if (terminal == NULL || terminal->rendering_enabled == enabled) {
        return;
    }
    terminal->rendering_enabled = enabled;
    if (enabled) {
        terminal_redraw(terminal);
    }
}

void terminal_write(terminal_t* terminal, const char* text)
{
    if (terminal == NULL || terminal->cells == NULL || text == NULL) {
        return;
    }
    mark_cursor(terminal);
    terminal->cursor_phase_visible = true;
    while (*text != '\0') {
        if (terminal->ansi_state == 1U) {
            if (*text == '[') {
                terminal->ansi_state      = 2U;
                terminal->ansi_value      = 0U;
                terminal->ansi_have_value = false;
            } else if (*text == '7') {
                terminal->saved_column = terminal->column;
                terminal->saved_line   = terminal->current_line;
                terminal->ansi_state   = 0U;
            } else if (*text == '8') {
                terminal->column       = terminal->saved_column;
                terminal->current_line = terminal->saved_line;
                terminal->ansi_state   = 0U;
            } else {
                terminal->ansi_state = 0U;
            }
            ++text;
            continue;
        }
        if (terminal->ansi_state == 2U) {
            if (*text >= '0' && *text <= '9') {
                terminal->ansi_value      = terminal->ansi_value * 10U + (unsigned int) (*text - '0');
                terminal->ansi_have_value = true;
                ++text;
                continue;
            }
            if (*text == ';') {
                ++text;
                continue;
            }
            ansi_command(terminal, *text);
            terminal->ansi_state = 0U;
            ++text;
            continue;
        }
        if ((unsigned char) *text == 0x1bU) {
            terminal->ansi_state = 1U;
            ++text;
            continue;
        }
        if (*text == '\n') {
            append_line(terminal, true);
        } else if (*text == '\r') {
            terminal->column = 0U;
        } else if (*text == '\b' || *text == 0x7f) {
            backspace(terminal);
        } else if (*text == '\t') {
            tab(terminal);
        } else {
            put_character(terminal, *text);
        }
        ++text;
    }
    follow_live_output(terminal);
    mark_cursor(terminal);
    render(terminal);
}

void terminal_write_line(terminal_t* terminal, const char* text)
{
    terminal_write(terminal, text);
    terminal_write(terminal, "\n");
}

void terminal_set_cursor_visible(terminal_t* terminal, bool visible)
{
    if (terminal != NULL && terminal->cursor_visible != visible) {
        mark_cursor(terminal);
        terminal->cursor_visible = visible;
        if (visible) {
            terminal->cursor_phase_visible = true;
        }
        mark_cursor(terminal);
        render(terminal);
    }
}

void terminal_set_cursor_phase(terminal_t* terminal, bool visible)
{
    if (terminal != NULL && terminal->cursor_phase_visible != visible) {
        mark_cursor(terminal);
        terminal->cursor_phase_visible = visible;
        mark_cursor(terminal);
        render(terminal);
    }
}

bool terminal_scroll_lines(terminal_t* terminal, int lines)
{
    if (terminal == NULL || terminal->cells == NULL) {
        return false;
    }
    const uint64_t maximum = live_viewport_top(terminal);
    int64_t desired        = (int64_t) terminal->viewport_top + lines;
    if (desired < (int64_t) terminal->first_line) {
        desired = (int64_t) terminal->first_line;
    }
    if ((uint64_t) desired > maximum) {
        desired = (int64_t) maximum;
    }
    if (terminal->viewport_top != (uint64_t) desired) {
        terminal->viewport_top = (uint64_t) desired;
        terminal->full_redraw  = true;
    }
    render(terminal);
    return true;
}

bool terminal_page_up(terminal_t* terminal)
{
    if (terminal == NULL || terminal->rows == 0U) {
        return false;
    }
    const size_t amount = terminal->rows > 1U ? terminal->rows - 1U : 1U;
    const int lines     = amount > (size_t) INT32_MAX ? INT32_MIN + 1 : -(int) amount;
    return terminal_scroll_lines(terminal, lines);
}

bool terminal_page_down(terminal_t* terminal)
{
    if (terminal == NULL || terminal->rows == 0U) {
        return false;
    }
    const size_t amount = terminal->rows > 1U ? terminal->rows - 1U : 1U;
    const int lines     = amount > (size_t) INT32_MAX ? INT32_MAX : (int) amount;
    return terminal_scroll_lines(terminal, lines);
}

bool terminal_scroll_to_start(terminal_t* terminal)
{
    if (terminal == NULL || terminal->cells == NULL) {
        return false;
    }
    if (terminal->viewport_top != terminal->first_line) {
        terminal->viewport_top = terminal->first_line;
        terminal->full_redraw  = true;
    }
    render(terminal);
    return true;
}

bool terminal_scroll_to_end(terminal_t* terminal)
{
    if (terminal == NULL || terminal->cells == NULL) {
        return false;
    }
    follow_live_output(terminal);
    render(terminal);
    return true;
}

bool terminal_is_at_end(const terminal_t* terminal)
{
    return terminal != NULL && terminal->cells != NULL && terminal->viewport_top == live_viewport_top(terminal);
}

size_t terminal_history_line_count(const terminal_t* terminal)
{
    return terminal == NULL || terminal->cells == NULL ? 0U :
                                                         (size_t) (terminal->current_line - terminal->first_line + 1U);
}
