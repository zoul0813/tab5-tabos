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
static tabos_process_id_t entry_process_id;

static bool test_entry(tabos_app_context_t *context)
{
    ++entry_calls;
    entry_process_id = tabos_app_process_id(context);
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
    input_init();
    if (!display_init()) {
        return 1;
    }

    terminal_t terminal;
    if (!terminal_init(&terminal, display_framebuffer(), 2U)) {
        return 1;
    }
    console_init(&terminal);
    kernel_application_system_init();

    tabos_app_descriptor_t invalid = test_app;
    invalid.abi_version = TABOS_APPLICATION_ABI_VERSION + 1U;
    if (application_registry_register(&invalid) ||
        !application_registry_register(&test_app) ||
        application_registry_register(&test_app) ||
        !application_registry_register(&failing_app) ||
        tabos_app_count() != 2U || tabos_app_at(2U) != NULL ||
        tabos_app_find("lifecycle-test") != &test_app ||
        tabos_app_launch(NULL) != TABOS_APP_RESULT_INVALID ||
        tabos_app_launch("missing") != TABOS_APP_RESULT_NOT_FOUND) {
        return 1;
    }

    int exit_status = 0;
    if (tabos_app_launch("failing-test") != TABOS_APP_RESULT_START_FAILED ||
        tabos_process_count() != 0U || cleanup_calls != 1U || cleanup_status != -1 ||
        !tabos_app_last_exit_status(&exit_status) || exit_status != -1) {
        return 1;
    }

    if (tabos_app_launch("lifecycle-test") != TABOS_APP_RESULT_OK ||
        !tabos_app_is_running() || tabos_app_active() != &test_app ||
        entry_calls != 1U || entry_process_id != 0U || tabos_process_count() != 1U ||
        tabos_app_launch("failing-test") != TABOS_APP_RESULT_BUSY) {
        return 1;
    }

    tabos_process_info_t process_info;
    if (!tabos_process_info(0U, &process_info) || process_info.id != 0U ||
        process_info.parent_id != TABOS_PROCESS_ID_INVALID ||
        process_info.state != TABOS_PROCESS_RUNNING ||
        process_info.name != test_app.name || tabos_process_info(1U, &process_info)) {
        return 1;
    }

    tabos_console_session_t denied = {0};
    if (tabos_console_acquire(&denied)) {
        return 1;
    }

    kernel_application_system_update();
    if (tabos_app_is_running() || tabos_app_active() != &test_app || update_calls != 1U ||
        cleanup_calls != 1U || tabos_process_count() != 1U ||
        !tabos_process_system_panicked() ||
        !tabos_process_info(0U, &process_info) ||
        process_info.state != TABOS_PROCESS_PANICKED ||
        !tabos_app_last_exit_status(&exit_status) || exit_status != 7 ||
        tabos_console_acquire(&denied)) {
        return 1;
    }

    kernel_application_system_shutdown();
    if (tabos_app_count() != 0U || tabos_process_count() != 0U ||
        cleanup_calls != 2U || cleanup_status != 7 ||
        !tabos_console_acquire(&denied)) {
        return 1;
    }
    tabos_console_release(&denied);
    console_shutdown();
    terminal_shutdown(&terminal);
    display_shutdown();
    input_shutdown();
    return 0;
}
