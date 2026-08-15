#include <tabos/internal/console_diagnostic.h>
#include <tabos/internal/application.h>

#include <tabos/tabos.h>

static tabos_app_context_t *app_context;
static const tabos_console_session_t *console;
static size_t editable_cells;

static void write_tab(void)
{
    size_t column = 0U;
    size_t row = 0U;
    if (tabos_console_get_cursor(console, &column, &row)) {
        (void)row;
        editable_cells += 4U - (column % 4U);
    }
    (void)tabos_console_write(console, "\t");
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
            (void)tabos_console_write(console, character);
            ++editable_cells;
        }
    }
}

static bool diagnostic_entry(tabos_app_context_t *context)
{
    app_context = context;
    console = tabos_app_console(context);
    if (console == NULL) {
        return false;
    }
    editable_cells = 0U;
    return tabos_console_write(
        console,
        "\nConsole test: Type to test keyboard and terminal\n"
        "Console test: Backspace edits; Ctrl+L clears\n"
        "Console test: Ctrl+arrows navigate history\n"
        "Console test: Ctrl+Q reports completion\n\n> "
    );
}

static void diagnostic_update(tabos_app_context_t *context)
{
    if (context != app_context || console == NULL) {
        return;
    }

    tabos_input_event_t event;
    while (tabos_console_poll(console, &event)) {
        if (event.type == TABOS_INPUT_TEXT) {
            write_text_event(event.text);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_ENTER) {
            (void)tabos_console_write(console, "\n> ");
            editable_cells = 0U;
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_TAB) {
            write_tab();
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_UP &&
                   (event.modifiers & TABOS_MODIFIER_CONTROL) != 0U) {
            (void)tabos_console_page_up(console);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_DOWN &&
                   (event.modifiers & TABOS_MODIFIER_CONTROL) != 0U) {
            (void)tabos_console_page_down(console);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_LEFT &&
                   (event.modifiers & TABOS_MODIFIER_CONTROL) != 0U) {
            (void)tabos_console_scroll_to_start(console);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_RIGHT &&
                   (event.modifiers & TABOS_MODIFIER_CONTROL) != 0U) {
            (void)tabos_console_scroll_to_end(console);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_PAGE_UP) {
            (void)tabos_console_page_up(console);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_PAGE_DOWN) {
            (void)tabos_console_page_down(console);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_HOME) {
            (void)tabos_console_scroll_to_start(console);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_END) {
            (void)tabos_console_scroll_to_end(console);
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_BACKSPACE &&
                   editable_cells > 0U) {
            (void)tabos_console_write(console, "\b");
            --editable_cells;
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_L &&
                   (event.modifiers & TABOS_MODIFIER_CONTROL) != 0U) {
            if (tabos_console_clear(console)) {
                (void)tabos_console_write(console, "> ");
                editable_cells = 0U;
            }
        } else if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_Q &&
                   (event.modifiers & TABOS_MODIFIER_CONTROL) != 0U) {
            (void)tabos_console_write(console,
                "\nConsole diagnostic complete; PID 0 remains active\n> ");
            tab_app_report_diagnostic_result(context, 0);
            editable_cells = 0U;
            return;
        }
    }
}

static void diagnostic_cleanup(tabos_app_context_t *context, int exit_status)
{
    (void)context;
    (void)exit_status;
    app_context = NULL;
    console = NULL;
    editable_cells = 0U;
}

const tabos_app_descriptor_t tab_console_diagnostic_app = {
    .abi_version = TABOS_APPLICATION_ABI_VERSION,
    .name = "console-test",
    .version = "1.0.0",
    .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
    .entry = diagnostic_entry,
    .update = diagnostic_update,
    .cleanup = diagnostic_cleanup,
};
