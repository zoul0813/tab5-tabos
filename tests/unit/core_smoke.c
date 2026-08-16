#include <tabos/internal/runtime.h>
#include <tabos/internal/input.h>

#include <tabos/application.h>
#include <tabos/terminal.h>

#include <tabos/config/console.h>
#include <tabos/config/filesystem.h>
#include <tabos/config/loader.h>

#include <string.h>

int main(void)
{
    if (!kernel_runtime_init()) {
        return 1;
    }

    if (!kernel_runtime_init()) {
        return 1;
    }

    if (strlen(kernel_runtime_version()) == 0U) {
        return 1;
    }

    if (tabos_terminal_set_scale(0U) || tabos_terminal_set_scale(9U) ||
        !tabos_terminal_set_scale(3U) || tabos_terminal_get_scale() != 3U) {
        return 1;
    }

#if TABOS_ENABLE_SHELL_STARTUP
    if (kernel_runtime_start()) {
        return 1;
    }
    kernel_runtime_shutdown();
    return 0;
#else
    if (!kernel_runtime_start()) {
        return 1;
    }
#endif

#if TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP
    const tabos_app_descriptor_t *active = tabos_app_active();
    if (active == NULL || strcmp(active->name, "console-test") != 0) {
        return 1;
    }
    const tabos_input_event_t exit_event = {
        .type = TABOS_INPUT_KEY_DOWN,
        .key = TABOS_KEY_Q,
        .modifiers = TABOS_MODIFIER_CONTROL,
    };
    if (!input_submit(&exit_event)) {
        return 1;
    }
    kernel_runtime_update();
    int exit_status = -1;
    if (!tabos_app_is_running() || tabos_process_count() != 1U ||
        tabos_process_system_panicked() ||
        !tabos_app_last_exit_status(&exit_status) || exit_status != 0) {
        return 1;
    }
#elif TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP
    int exit_status = 0;
    if (tabos_app_count() != 1U || !tabos_app_is_running() ||
        tabos_process_count() != 1U || tabos_process_system_panicked() ||
        !tabos_app_last_exit_status(&exit_status) || exit_status != 1) {
        return 1;
    }
#elif TABOS_ENABLE_ELF_LOADER_EXPERIMENT
    int exit_status = 0;
    if (tabos_app_count() != 1U || tabos_app_is_running() ||
        tabos_process_count() != 1U || !tabos_process_system_panicked() ||
        !tabos_app_last_exit_status(&exit_status) || exit_status != 4) {
        return 1;
    }
#else
    if (tabos_app_count() != 0U || tabos_app_is_running()) {
        return 1;
    }
#endif

    if (!tabos_terminal_set_scale(4U) || tabos_terminal_get_scale() != 4U) {
        return 1;
    }

    kernel_runtime_shutdown();
    return 0;
}
