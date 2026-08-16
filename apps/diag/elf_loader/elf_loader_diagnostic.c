#include <tabos/internal/elf_loader_diagnostic.h>

#include <tabos/config/loader.h>

#include <stdio.h>

static unsigned int launch_count;
static bool diagnostic_complete;

static bool launch_hello(tabos_app_context_t *context)
{
    const tabos_console_session_t *console = tabos_app_console(context);
    if (console == NULL) return false;

    char message[600];
    (void)snprintf(message, sizeof(message), "Launching %s", TABOS_ELF_STARTUP_PATH);
    (void)tabos_console_write_line(console, message);
    launch_count++;
    const tabos_app_result_t result = tabos_app_exec(context, TABOS_ELF_STARTUP_PATH);
    if (result == TABOS_APP_RESULT_OK) return true;

    (void)snprintf(message, sizeof(message), "ELF launch FAILED: %d", (int)result);
    (void)tabos_console_write_line(console, message);
    diagnostic_complete = true;
    return true;
}

static bool launcher_entry(tabos_app_context_t *context)
{
    launch_count = 0U;
    diagnostic_complete = false;
    return launch_hello(context);
}

static void launcher_update(tabos_app_context_t *context)
{
    if (diagnostic_complete) return;
    int status = 0;
    if (!tabos_app_take_child_status(context, &status)) return;

    const tabos_console_session_t *console = tabos_app_console(context);
    if (console == NULL) return;
    char message[64];
    (void)snprintf(message, sizeof(message), "ELF child exited with status %d", status);
    (void)tabos_console_write_line(console, message);
    if (status != 0) {
        (void)tabos_console_write_line(console, "ELF launcher diagnostic FAILED");
        diagnostic_complete = true;
        return;
    }
    if (launch_count < 2U) {
        (void)launch_hello(context);
        return;
    }
    (void)tabos_console_write_line(console, "ELF launcher diagnostic passed");
    diagnostic_complete = true;
}

const tabos_app_descriptor_t elf_loader_diagnostic_app = {
    .abi_version = TABOS_APPLICATION_ABI_VERSION,
    .name = "elf-hello",
    .version = "0.2.0",
    .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
    .entry = launcher_entry,
    .update = launcher_update,
};
