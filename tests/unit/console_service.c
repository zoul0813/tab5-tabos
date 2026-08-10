#include <tabos/console.h>

#include <tabos/internal/console.h>
#include <tabos/internal/display.h>
#include <tabos/internal/input.h>
#include <tabos/internal/terminal.h>

#include "platform_test.h"

#include <stdlib.h>

int main(void)
{
    tab_input_init();
    if (!tab_display_init()) {
        return 1;
    }

    tab_terminal_t terminal;
    if (!tab_terminal_init(&terminal, tab_display_framebuffer(), 2U)) {
        return 1;
    }
    tab_terminal_clear(&terminal);
    tab_console_init(&terminal);

    tabos_console_session_t foreground = {0};
    tabos_console_session_t background = {0};
    if (!tabos_console_acquire(&foreground) || tabos_console_acquire(&background) ||
        !tabos_console_is_foreground(&foreground) ||
        tabos_console_is_foreground(&background)) {
        return 1;
    }
    if (!terminal.cursor_visible ||
        tab_display_framebuffer()->pixels[0] != 0xffff) {
        return 1;
    }
    tab_test_platform_advance_time_ms(500U);
    tab_console_update();
    if (terminal.cursor_phase_visible) {
        return 1;
    }
    tab_test_platform_advance_time_ms(500U);
    tab_console_update();
    if (!terminal.cursor_phase_visible) {
        return 1;
    }

    if (!tabos_console_write(&foreground, "AB\tC") ||
        !tabos_console_write(&foreground, "\b\n")) {
        return 1;
    }
    size_t column = 99U;
    size_t row = 99U;
    if (!tabos_console_get_cursor(&foreground, &column, &row) || column != 0U || row != 1U) {
        return 1;
    }

    const tabos_input_event_t submitted = {
        .type = TABOS_INPUT_KEY_DOWN,
        .key = TABOS_KEY_A,
    };
    if (!tab_input_submit(&submitted)) {
        return 1;
    }
    tabos_input_event_t received;
    if (tabos_console_poll(&background, &received) ||
        !tabos_console_poll(&foreground, &received) || received.key != TABOS_KEY_A) {
        return 1;
    }

    tabos_console_release(&foreground);
    if (tabos_console_is_foreground(&foreground) ||
        tabos_console_write(&foreground, "stale") ||
        !tabos_console_acquire(&background) || !tabos_console_clear(&background) ||
        !tabos_console_get_cursor(&background, &column, &row) || column != 0U || row != 0U) {
        return 1;
    }

    const size_t generated_lines = terminal.line_capacity + 10U;
    char *history = malloc((generated_lines * 2U) + 1U);
    if (history == NULL) {
        return 1;
    }
    for (size_t index = 0U; index < generated_lines; ++index) {
        history[index * 2U] = 'x';
        history[(index * 2U) + 1U] = '\n';
    }
    history[generated_lines * 2U] = '\0';
    const bool history_written = tabos_console_write(&background, history);
    free(history);
    size_t history_lines = 0U;
    if (!history_written ||
        !tabos_console_get_history_line_count(&background, &history_lines) ||
        history_lines != terminal.line_capacity || terminal.first_line == 0U) {
        return 1;
    }
    if (!tabos_console_get_cursor(&background, &column, &row) ||
        row != terminal.rows - 1U) {
        return 1;
    }
    if (!tabos_console_page_up(&background) || tabos_console_is_at_end(&background) ||
        !tabos_console_page_down(&background) ||
        !tabos_console_scroll_to_start(&background) ||
        terminal.viewport_top != terminal.first_line ||
        !tabos_console_scroll_to_end(&background) ||
        !tabos_console_is_at_end(&background)) {
        return 1;
    }
    if (!tabos_console_page_up(&background) ||
        !tabos_console_write(&background, "live") ||
        !tabos_console_is_at_end(&background)) {
        return 1;
    }

    if (!tabos_console_clear(&background) ||
        !tabos_console_write(&background, "SCALE") ||
        !tab_terminal_resize(&terminal, tab_display_framebuffer(), 4U) ||
        terminal.scale != 4U || terminal.column != 5U ||
        terminal.cells[0].character != 'S' || terminal.cells[4].character != 'E') {
        return 1;
    }

    tabos_console_release(&background);
    tab_console_shutdown();
    tab_terminal_shutdown(&terminal);
    tab_display_shutdown();
    tab_input_shutdown();
    return 0;
}
