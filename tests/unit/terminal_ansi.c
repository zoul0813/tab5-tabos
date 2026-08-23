#include <tabos/internal/terminal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "terminal ANSI test failed: %s\n", message);
        exit(1);
    }
}

static terminal_cell_t* cell(terminal_t* terminal, size_t column, size_t row)
{
    return &terminal->cells[(row * terminal->columns) + column];
}

int main(void)
{
    enum {
        WIDTH  = 160,
        HEIGHT = 60
    };
    platform_pixel_t pixels[WIDTH * HEIGHT];
    platform_framebuffer_t framebuffer = {
        .pixels        = pixels,
        .width         = WIDTH,
        .height        = HEIGHT,
        .stride_pixels = WIDTH,
    };
    terminal_t terminal;
    check(terminal_init(&terminal, &framebuffer, 1U), "initialize");

    terminal_write(&terminal, "abc");
    terminal_write(&terminal, "\033[2J\033[H");
    check(cell(&terminal, 0U, 0U)->character == '\0', "clear screen");
    check(terminal.column == 0U, "home column");

    terminal_write(&terminal, "A\033[2CB");
    check(cell(&terminal, 0U, 0U)->character == 'A', "cursor movement source");
    check(cell(&terminal, 3U, 0U)->character == 'B', "cursor movement destination");

    terminal_write(&terminal, "\033[31mR\033[0mN");
    check(cell(&terminal, 3U, 0U)->foreground != cell(&terminal, 4U, 0U)->foreground, "foreground color");

    terminal_write(&terminal, "\033[2Hxy\033[s\033[3Cz\033[uQ");
    check(cell(&terminal, 0U, 1U)->character == 'x', "row positioning");
    check(cell(&terminal, 1U, 1U)->character == 'y', "positioned text");
    check(cell(&terminal, 2U, 1U)->character == 'Q', "cursor restore");

    terminal_write(&terminal, "\033[2K");
    check(cell(&terminal, 0U, 1U)->character == '\0', "erase line");
    check(cell(&terminal, 1U, 1U)->character == '\0', "erase line contents");

    terminal_write(&terminal, "\033");
    terminal_write(&terminal, "[HZ");
    check(cell(&terminal, 0U, 0U)->character == 'Z', "split escape sequence");

    terminal_shutdown(&terminal);
    return 0;
}
