#include <tabos/console.h>

#include <tabos/internal/console.h>
#include <tabos/internal/display.h>
#include <tabos/internal/input.h>
#include <tabos/internal/terminal.h>

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

    for (size_t index = 0U; index < terminal.rows + 2U; ++index) {
        if (!tabos_console_write(&background, "line\n")) {
            return 1;
        }
    }
    if (!tabos_console_get_cursor(&background, &column, &row) ||
        row != terminal.rows - 1U) {
        return 1;
    }

    tabos_console_release(&background);
    tab_console_shutdown();
    tab_display_shutdown();
    tab_input_shutdown();
    return 0;
}
