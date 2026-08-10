#include <tabos/internal/terminal.h>

#include <tabos/internal/font.h>

#include <string.h>

static size_t cell_width(const tab_terminal_t *terminal)
{
    return (TAB_FONT_GLYPH_WIDTH + 1U) * terminal->scale;
}

static size_t cell_height(const tab_terminal_t *terminal)
{
    return (TAB_FONT_GLYPH_HEIGHT + 1U) * terminal->scale;
}

static void scroll(tab_terminal_t *terminal)
{
    const size_t height = cell_height(terminal);
    const size_t stride = terminal->framebuffer->stride_pixels;
    const size_t retained_rows = terminal->framebuffer->height - height;
    memmove(terminal->framebuffer->pixels,
            terminal->framebuffer->pixels + height * stride,
            retained_rows * stride * sizeof(tab_pixel_t));
    for (size_t y = retained_rows; y < terminal->framebuffer->height; ++y) {
        for (size_t x = 0; x < terminal->framebuffer->width; ++x) {
            terminal->framebuffer->pixels[y * stride + x] = terminal->background;
        }
    }
    terminal->row = terminal->rows - 1U;
}

static void newline(tab_terminal_t *terminal)
{
    terminal->column = 0U;
    ++terminal->row;
    if (terminal->row >= terminal->rows) {
        scroll(terminal);
    }
}

static void draw_character(tab_terminal_t *terminal, char character)
{
    (void)tab_font_draw_char(terminal->framebuffer,
                             (int)(terminal->column * cell_width(terminal)),
                             (int)(terminal->row * cell_height(terminal)), character,
                             terminal->scale, terminal->foreground, terminal->background);
}

static void backspace(tab_terminal_t *terminal)
{
    if (terminal->column > 0U) {
        --terminal->column;
    } else if (terminal->row > 0U) {
        --terminal->row;
        terminal->column = terminal->columns - 1U;
    } else {
        return;
    }
    draw_character(terminal, ' ');
}

static void tab(tab_terminal_t *terminal)
{
    enum { TAB_WIDTH = 4 };
    const size_t spaces = TAB_WIDTH - (terminal->column % TAB_WIDTH);
    for (size_t index = 0U; index < spaces; ++index) {
        draw_character(terminal, ' ');
        ++terminal->column;
        if (terminal->column >= terminal->columns) {
            newline(terminal);
        }
    }
}

bool tab_terminal_init(tab_terminal_t *terminal, tab_framebuffer_t *framebuffer, unsigned int scale)
{
    if (terminal == NULL || framebuffer == NULL || framebuffer->pixels == NULL || scale == 0U) {
        return false;
    }
    *terminal = (tab_terminal_t){
        .framebuffer = framebuffer,
        .columns = framebuffer->width / ((TAB_FONT_GLYPH_WIDTH + 1U) * scale),
        .rows = framebuffer->height / ((TAB_FONT_GLYPH_HEIGHT + 1U) * scale),
        .scale = scale,
        .foreground = 0xffff,
        .background = 0x0000,
    };
    return terminal->columns > 0U && terminal->rows > 0U;
}

void tab_terminal_clear(tab_terminal_t *terminal)
{
    if (terminal == NULL || terminal->framebuffer == NULL) {
        return;
    }
    for (size_t y = 0; y < terminal->framebuffer->height; ++y) {
        for (size_t x = 0; x < terminal->framebuffer->width; ++x) {
            terminal->framebuffer->pixels[y * terminal->framebuffer->stride_pixels + x] =
                terminal->background;
        }
    }
    terminal->column = 0U;
    terminal->row = 0U;
}

void tab_terminal_set_colors(tab_terminal_t *terminal, tab_pixel_t foreground,
                             tab_pixel_t background)
{
    if (terminal != NULL) {
        terminal->foreground = foreground;
        terminal->background = background;
    }
}

void tab_terminal_write(tab_terminal_t *terminal, const char *text)
{
    if (terminal == NULL || terminal->framebuffer == NULL || text == NULL) {
        return;
    }
    while (*text != '\0') {
        if (*text == '\n') {
            newline(terminal);
        } else if (*text == '\r') {
            terminal->column = 0U;
        } else if (*text == '\b' || *text == 0x7f) {
            backspace(terminal);
        } else if (*text == '\t') {
            tab(terminal);
        } else {
            draw_character(terminal, *text);
            ++terminal->column;
            if (terminal->column >= terminal->columns) {
                newline(terminal);
            }
        }
        ++text;
    }
}

void tab_terminal_write_line(tab_terminal_t *terminal, const char *text)
{
    tab_terminal_write(terminal, text);
    tab_terminal_write(terminal, "\n");
}
