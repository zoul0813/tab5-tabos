#include <tabos/application.h>
#include <tabos/elf_api.h>

#include <tabos/internal/application.h>
#include <tabos/internal/console.h>
#include <tabos/internal/display.h>
#include <tabos/internal/input.h>
#include <tabos/internal/terminal.h>

#include "platform_test.h"

#include <string.h>

static unsigned int entry_calls;
static unsigned int update_calls;
static unsigned int cleanup_calls;
static int cleanup_status;
static tabos_process_id_t entry_process_id;
static unsigned int root_stage;
static unsigned int child_stage;
static bool nested_complete;
static unsigned int file_root_stage;
static bool file_child_cleanup_complete;
static tabos_process_termination_t requested_panic_cause;

static bool terminal_contains(const terminal_t *terminal, const char *text)
{
    const size_t length = strlen(text);
    if (length == 0U) return true;
    size_t matched = 0U;
    const size_t cell_count = terminal->line_capacity * terminal->columns;
    for (size_t index = 0U; index < cell_count; ++index) {
        if (terminal->cells[index].character == text[matched]) {
            if (++matched == length) return true;
        } else {
            matched = terminal->cells[index].character == text[0] ? 1U : 0U;
        }
    }
    return false;
}

static bool nested_entry(tabos_app_context_t *context)
{
    return tabos_app_console(context) != NULL;
}

static void root_update(tabos_app_context_t *context)
{
    if (root_stage++ == 0U) {
        if (kernel_process_launch_child(context, "nested-child") != TABOS_APP_RESULT_OK) {
            tabos_app_request_exit(context, 90);
        }
        return;
    }
    int status = 0;
    if (tabos_app_take_child_status(context, &status) && status == 22) {
        nested_complete = true;
    }
}

static void child_update(tabos_app_context_t *context)
{
    if (child_stage++ == 0U) {
        if (kernel_process_launch_child(context, "nested-grandchild") != TABOS_APP_RESULT_OK) {
            tabos_app_request_exit(context, 91);
        }
        return;
    }
    int status = 0;
    if (tabos_app_take_child_status(context, &status) && status == 33) {
        tabos_app_request_exit(context, 22);
    }
}

static void grandchild_update(tabos_app_context_t *context)
{
    tabos_app_request_exit(context, 33);
}

static const tabos_app_descriptor_t root_app = {
    .abi_version = TABOS_APPLICATION_ABI_VERSION,
    .name = "nested-root",
    .version = "1.0.0",
    .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
    .entry = nested_entry,
    .update = root_update,
};

static const tabos_app_descriptor_t child_app = {
    .abi_version = TABOS_APPLICATION_ABI_VERSION,
    .name = "nested-child",
    .version = "1.0.0",
    .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
    .entry = nested_entry,
    .update = child_update,
};

static const tabos_app_descriptor_t grandchild_app = {
    .abi_version = TABOS_APPLICATION_ABI_VERSION,
    .name = "nested-grandchild",
    .version = "1.0.0",
    .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
    .entry = nested_entry,
    .update = grandchild_update,
};

static void file_root_update(tabos_app_context_t *context)
{
    if (file_root_stage++ == 0U) {
        if (tabos_app_exec(context, "T:/missing.bin") != TABOS_APP_RESULT_START_FAILED) {
            tabos_app_request_exit(context, 92);
        }
        return;
    }
    int status = 0;
    if (tabos_app_take_child_status(context, &status) && status == -1) {
        file_child_cleanup_complete = true;
    }
}

static const tabos_app_descriptor_t file_root_app = {
    .abi_version = TABOS_APPLICATION_ABI_VERSION,
    .name = "file-root",
    .version = "1.0.0",
    .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
    .entry = nested_entry,
    .update = file_root_update,
};

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

static void panic_update(tabos_app_context_t *context)
{
    kernel_process_fail(context, requested_panic_cause, 41);
}

static const tabos_app_descriptor_t panic_app = {
    .abi_version = TABOS_APPLICATION_ABI_VERSION,
    .name = "panic-test",
    .version = "1.0.0",
    .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
    .entry = nested_entry,
    .update = panic_update,
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
    if (!console_init(&terminal)) {
        return 1;
    }
    kernel_application_system_init();

    if (!application_registry_register(&root_app) ||
        !application_registry_register(&child_app) ||
        !application_registry_register(&grandchild_app) ||
        tabos_app_launch("nested-root") != TABOS_APP_RESULT_OK) {
        return 1;
    }
    kernel_application_system_update();
    tabos_process_info_t process_info;
    if (tabos_process_count() != 2U ||
        !tabos_process_info(0U, &process_info) ||
        process_info.state != TABOS_PROCESS_BLOCKED ||
        !tabos_process_info(1U, &process_info) || process_info.parent_id != 0U ||
        process_info.state != TABOS_PROCESS_RUNNING) {
        return 1;
    }
    kernel_application_system_update();
    if (tabos_process_count() != 3U ||
        !tabos_process_info(2U, &process_info) || process_info.parent_id != 1U ||
        process_info.state != TABOS_PROCESS_RUNNING) {
        return 1;
    }
    kernel_application_system_update();
    kernel_application_system_update();
    kernel_application_system_update();
    if (!nested_complete || tabos_process_count() != 1U ||
        !tabos_process_info(0U, &process_info) ||
        process_info.state != TABOS_PROCESS_RUNNING || tabos_process_system_panicked()) {
        return 1;
    }
    kernel_application_system_shutdown();
    kernel_application_system_init();

    if (!application_registry_register(&file_root_app) ||
        tabos_app_launch("file-root") != TABOS_APP_RESULT_OK) {
        return 1;
    }
    kernel_application_system_update();
    kernel_application_system_update();
    if (!file_child_cleanup_complete || tabos_process_count() != 1U ||
        !tabos_process_info(0U, &process_info) ||
        process_info.state != TABOS_PROCESS_RUNNING || tabos_process_system_panicked()) {
        return 1;
    }
    kernel_application_system_shutdown();
    kernel_application_system_init();

    if (tabos_app_launch_path("T:/missing-shell.bin") != TABOS_APP_RESULT_START_FAILED ||
        tabos_process_count() != 0U) {
        return 1;
    }
    const char *too_many_arguments[TABOS_ELF_ARG_MAX + 1U];
    for (size_t index = 0U; index < TABOS_ELF_ARG_MAX + 1U; ++index) {
        too_many_arguments[index] = "x";
    }
    char oversized_argument[TABOS_ELF_ARG_BYTES_MAX + 1U];
    for (size_t index = 0U; index < sizeof(oversized_argument) - 1U; ++index) {
        oversized_argument[index] = 'x';
    }
    oversized_argument[sizeof(oversized_argument) - 1U] = '\0';
    const char *oversized_arguments[] = {oversized_argument};
    if (tabos_app_launch_path_args("T:/missing-shell.bin", TABOS_ELF_ARG_MAX + 1U,
                                   too_many_arguments) != TABOS_APP_RESULT_INVALID ||
        tabos_app_launch_path_args("T:/missing-shell.bin", 1U,
                                   oversized_arguments) != TABOS_APP_RESULT_INVALID ||
        tabos_process_count() != 0U) {
        return 1;
    }

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
    tabos_process_termination_t panic_cause = TABOS_PROCESS_TERMINATION_NONE;
    if (tabos_app_is_running() || tabos_app_active() != &test_app || update_calls != 1U ||
        cleanup_calls != 1U || tabos_process_count() != 1U ||
        !tabos_process_system_panicked() ||
        !tabos_process_info(0U, &process_info) ||
        process_info.state != TABOS_PROCESS_PANICKED ||
        !tabos_app_last_exit_status(&exit_status) || exit_status != 7 ||
        !tabos_process_panic_info(&panic_cause, &exit_status) ||
        panic_cause != TABOS_PROCESS_TERMINATION_EXIT_REQUEST || exit_status != 7 ||
        strstr(test_platform_last_log(), "exit request") == NULL ||
        !terminal_contains(&terminal, "KERNEL PANIC") ||
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

    const tabos_process_termination_t panic_causes[] = {
        TABOS_PROCESS_TERMINATION_RETURN,
        TABOS_PROCESS_TERMINATION_FAULT,
        TABOS_PROCESS_TERMINATION_FORCED,
    };
    for (size_t index = 0U; index < sizeof(panic_causes) / sizeof(panic_causes[0]); ++index) {
        kernel_application_system_init();
        if (!application_registry_register(&panic_app) ||
            tabos_app_launch("panic-test") != TABOS_APP_RESULT_OK) {
            return 1;
        }
        requested_panic_cause = panic_causes[index];
        test_platform_clear_log();
        if (requested_panic_cause == TABOS_PROCESS_TERMINATION_FORCED) {
            if (!kernel_process_force_terminate(0U, 41)) return 1;
        }
        kernel_application_system_update();
        if (!tabos_process_panic_info(&panic_cause, &exit_status) ||
            panic_cause != requested_panic_cause || exit_status != 41 ||
            tabos_process_count() != 1U || strlen(test_platform_last_log()) == 0U) {
            return 1;
        }
        kernel_application_system_shutdown();
    }

    console_shutdown();
    terminal_shutdown(&terminal);
    display_shutdown();
    input_shutdown();
    return 0;
}
