#include <tabos/application.h>

#include <tabos/internal/application.h>
#include <tabos/internal/console.h>
#include <tabos/internal/display.h>
#include <tabos/internal/input.h>
#include <tabos/internal/terminal.h>

static unsigned int entry_calls;
static unsigned int update_calls;
static unsigned int cleanup_calls;
static int cleanup_status;

static bool test_entry(tabos_app_context_t *context)
{
    ++entry_calls;
    const tabos_console_session_t *console = tabos_app_console(context);
    return console != NULL && tabos_console_write(console, "APP STARTED");
}

static void test_update(tabos_app_context_t *context)
{
    ++update_calls;
    tabos_app_request_exit(context, 7);
}

static void test_cleanup(tabos_app_context_t *context, int exit_status)
{
    (void)context;
    ++cleanup_calls;
    cleanup_status = exit_status;
}

static bool failing_entry(tabos_app_context_t *context)
{
    (void)context;
    return false;
}

static const tabos_app_descriptor_t test_app = {
    .abi_version = TABOS_APPLICATION_ABI_VERSION,
    .name = "lifecycle-test",
    .version = "1.0.0",
    .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
    .entry = test_entry,
    .update = test_update,
    .cleanup = test_cleanup,
};

static const tabos_app_descriptor_t failing_app = {
    .abi_version = TABOS_APPLICATION_ABI_VERSION,
    .name = "failing-test",
    .version = "1.0.0",
    .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
    .entry = failing_entry,
    .cleanup = test_cleanup,
};

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
    tab_console_init(&terminal);
    tab_app_system_init();

    tabos_app_descriptor_t invalid = test_app;
    invalid.abi_version = TABOS_APPLICATION_ABI_VERSION + 1U;
    if (tab_app_registry_register(&invalid) ||
        !tab_app_registry_register(&test_app) ||
        tab_app_registry_register(&test_app) ||
        !tab_app_registry_register(&failing_app) ||
        tabos_app_count() != 2U || tabos_app_at(2U) != NULL ||
        tabos_app_find("lifecycle-test") != &test_app ||
        tabos_app_launch(NULL) != TABOS_APP_RESULT_INVALID ||
        tabos_app_launch("missing") != TABOS_APP_RESULT_NOT_FOUND) {
        return 1;
    }

    if (tabos_app_launch("lifecycle-test") != TABOS_APP_RESULT_OK ||
        !tabos_app_is_running() || tabos_app_active() != &test_app ||
        entry_calls != 1U ||
        tabos_app_launch("failing-test") != TABOS_APP_RESULT_BUSY) {
        return 1;
    }

    tabos_console_session_t denied = {0};
    if (tabos_console_acquire(&denied)) {
        return 1;
    }

    tab_app_system_update();
    int exit_status = 0;
    if (tabos_app_is_running() || tabos_app_active() != NULL || update_calls != 1U ||
        cleanup_calls != 1U || cleanup_status != 7 ||
        !tabos_app_last_exit_status(&exit_status) || exit_status != 7 ||
        !tabos_console_acquire(&denied)) {
        return 1;
    }
    tabos_console_release(&denied);

    if (tabos_app_launch("failing-test") != TABOS_APP_RESULT_START_FAILED ||
        tabos_app_is_running() || cleanup_calls != 2U || cleanup_status != -1 ||
        !tabos_app_last_exit_status(&exit_status) || exit_status != -1 ||
        !tabos_console_acquire(&denied)) {
        return 1;
    }
    tabos_console_release(&denied);

    tab_app_system_shutdown();
    if (tabos_app_count() != 0U) {
        return 1;
    }
    tab_console_shutdown();
    tab_terminal_shutdown(&terminal);
    tab_display_shutdown();
    tab_input_shutdown();
    return 0;
}
