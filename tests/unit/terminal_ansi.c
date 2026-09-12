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
    static const char embedded_nul[] = {'A', '\0', 'B'};
    terminal_write_bytes(&terminal, embedded_nul, sizeof(embedded_nul));
    check(cell(&terminal, 3U, 0U)->character == 'A' && cell(&terminal, 4U, 0U)->character == 'B',
          "embedded NUL does not terminate byte write");
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

    terminal_write(&terminal, "\033[2;5H\033[31;44mX\033[39;49mY");
    check(cell(&terminal, 4U, 1U)->character == 'X', "two parameter cursor");
    check(cell(&terminal, 4U, 1U)->background == 0x001f, "multiple SGR");
    check(cell(&terminal, 5U, 1U)->foreground == 0xffff && cell(&terminal, 5U, 1U)->background == 0, "default colors");
    terminal_write(&terminal, "\033[?");
    terminal_write(&terminal, "2");
    terminal_write(&terminal, "5l");
    check(!terminal.cursor_visible, "split private cursor hide");
    terminal_write(&terminal, "\033[?25h");
    check(terminal.cursor_visible, "cursor show");
    const size_t old_column = terminal.column;
    terminal_write(&terminal, "\033[999999999999999999999999C\033[1;2;3;4;5;6;7;8;9m");
    check(terminal.column == old_column, "overflow consumed without text or movement");
    for (size_t index = 0; index < 400U; ++index) {
        terminal_write(&terminal, "scroll\n");
    }
    const uint64_t top = terminal.viewport_top;
    terminal_write(&terminal, "\033[1;2HZ");
    check(terminal.viewport_top == top && terminal.current_line == top, "address live screen after ring overflow");
    check(terminal.cells[(top % terminal.line_capacity) * terminal.columns + 1U].character == 'Z',
          "ring positioned cell");
    terminal_write(&terminal, "\033[4;19HB");
    check(terminal.viewport_top == top, "last row painter leaves final column unused without scroll");
    terminal_write(&terminal, "C");
    check(terminal.viewport_top == top + 1U, "existing immediate wrap contract at bottom right");
    terminal_clear(&terminal);
    terminal_write(&terminal, "a\nb\n\b");
    check(terminal.last_line == terminal.current_line && terminal.current_line == 1U, "backspace trims live tail");
    terminal_write(&terminal, "\033[;5fD");
    check(terminal.current_line == 0U && terminal.column == 5U, "omitted row defaults to one");
    terminal_shutdown(&terminal);
    return 0;
}
