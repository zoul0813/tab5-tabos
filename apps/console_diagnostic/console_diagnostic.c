#include <tabos/internal/console_diagnostic.h>

#include <tabos/console.h>

static tabos_console_session_t console;
static bool running;
static size_t editable_cells;

static void write_tab(void)
{
    size_t column = 0U;
    size_t row = 0U;
    if (tabos_console_get_cursor(&console, &column, &row)) {
        (void)row;
        editable_cells += 4U - (column % 4U);
    }
    (void)tabos_console_write(&console, "\t");
}

static void write_text_event(const char *text)
{
    for (size_t index = 0U; text[index] != '\0'; ++index) {
        if (text[index] == '\n' || text[index] == '\r') {
            continue;
        } else if (text[index] == '\t') {
            continue;
        } else {
            char character[2] = {text[index], '\0'};
            (void)tabos_console_write(&console, character);
            ++editable_cells;
        }
    }
}

bool tab_console_diagnostic_start(void)
{
    if (running) {
        return true;
    }
    if (!tabos_console_acquire(&console)) {
        return false;
    }
    running = true;
    editable_cells = 0U;
    return tabos_console_write(
        &console,
        "\n[CONSOLE TEST] TYPE TO TEST KEYBOARD AND TERMINAL\n"
        "[CONSOLE TEST] BACKSPACE EDITS; CTRL+L CLEARS\n"
        "[CONSOLE TEST] CTRL+ARROWS NAVIGATE HISTORY\n\n> "
    );
}

void tab_console_diagnostic_update(void)
{
    if (!running) {
        return;
    }

    tabos_input_event_t event;
    while (tabos_console_poll(&console, &event)) {
        if (event.type == TABOS_INPUT_TEXT) {
            write_text_event(event.text);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_ENTER) {
            (void)tabos_console_write(&console, "\n> ");
            editable_cells = 0U;
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_TAB) {
            write_tab();
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_UP &&
                   (event.modifiers & TABOS_MODIFIER_CONTROL) != 0U) {
            (void)tabos_console_page_up(&console);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_DOWN &&
                   (event.modifiers & TABOS_MODIFIER_CONTROL) != 0U) {
            (void)tabos_console_page_down(&console);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_LEFT &&
                   (event.modifiers & TABOS_MODIFIER_CONTROL) != 0U) {
            (void)tabos_console_scroll_to_start(&console);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_RIGHT &&
                   (event.modifiers & TABOS_MODIFIER_CONTROL) != 0U) {
            (void)tabos_console_scroll_to_end(&console);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_PAGE_UP) {
            (void)tabos_console_page_up(&console);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_PAGE_DOWN) {
            (void)tabos_console_page_down(&console);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_HOME) {
            (void)tabos_console_scroll_to_start(&console);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_END) {
            (void)tabos_console_scroll_to_end(&console);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_BACKSPACE &&
                   editable_cells > 0U) {
            (void)tabos_console_write(&console, "\b");
            --editable_cells;
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_L &&
                   (event.modifiers & TABOS_MODIFIER_CONTROL) != 0U) {
            if (tabos_console_clear(&console)) {
                (void)tabos_console_write(&console, "> ");
                editable_cells = 0U;
            }
        }
    }
}

void tab_console_diagnostic_stop(void)
{
    if (running) {
        tabos_console_release(&console);
        running = false;
        editable_cells = 0U;
    }
}
