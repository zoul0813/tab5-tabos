#include <tabos/internal/elf_application.h>

#include <tabos/elf_api.h>
#include <tabos/internal/elf_loader.h>

#include <tabos/platform/platform.h>

#include "fixtures/hello_elf.h"

#include <stdio.h>
#include <string.h>

static tabos_app_context_t *active_context;
static const tabos_console_session_t *active_console;
static tab_elf_image_t loaded_image;
static bool elf_exit_requested;

static void elf_console_write(const char *text)
{
    if (active_console != NULL && text != NULL) {
        const char *readable_text = tab_platform_executable_data_pointer(text);
        (void)tabos_console_write_line(active_console, readable_text);
    }
}

static void elf_request_exit(int exit_status)
{
    if (active_context != NULL) {
        elf_exit_requested = true;
        tabos_app_request_exit(active_context, exit_status);
    }
}

static bool elf_entry(tabos_app_context_t *context)
{
    active_context = context;
    active_console = tabos_app_console(context);
    elf_exit_requested = false;
    if (active_console == NULL) {
        return false;
    }

    const tab_elf_result_t result = tab_elf_load(tab_hello_elf, tab_hello_elf_size,
                                                 &loaded_image);
    if (result != TAB_ELF_OK) {
        char message[80];
        (void)snprintf(message, sizeof(message), "ELF LOAD FAILED: %s",
                       tab_elf_result_name(result));
        tab_platform_log(message);
        (void)tabos_console_write(active_console, "ELF LOAD FAILED: ");
        (void)tabos_console_write_line(active_console, tab_elf_result_name(result));
        tabos_app_request_exit(context, 3);
        return true;
    }
    if (!tab_platform_can_execute_riscv32()) {
        (void)tabos_console_write_line(active_console,
                                       "ELF EXECUTION UNSUPPORTED ON THIS HOST");
        tabos_app_request_exit(context, 4);
        return true;
    }

    char load_message[80];
    (void)snprintf(load_message, sizeof(load_message),
                   "ELF image loaded: %u file bytes, %u memory bytes",
                   (unsigned int)tab_hello_elf_size,
                   (unsigned int)loaded_image.memory_size);
    tab_platform_log(load_message);

    tabos_elf_entry_fn entry = NULL;
    _Static_assert(sizeof(entry) == sizeof(loaded_image.entry),
                   "ELF entry pointer must match data pointer size");
    memcpy(&entry, &loaded_image.entry, sizeof(entry));
    const tabos_elf_api_t api = {
        .abi_version = TABOS_ELF_API_VERSION,
        .console_write = elf_console_write,
        .request_exit = elf_request_exit,
    };
    const int returned_status = entry(&api);
    char exit_message[56];
    (void)snprintf(exit_message, sizeof(exit_message),
                   "ELF entry returned status %d", returned_status);
    tab_platform_log(exit_message);
    if (!elf_exit_requested) {
        tabos_app_request_exit(context, returned_status);
    }
    return true;
}

static void elf_cleanup(tabos_app_context_t *context, int exit_status)
{
    (void)context;
    (void)exit_status;
    tab_elf_unload(&loaded_image);
    active_context = NULL;
    active_console = NULL;
    elf_exit_requested = false;
}

const tabos_app_descriptor_t tab_elf_experiment_app = {
    .abi_version = TABOS_APPLICATION_ABI_VERSION,
    .name = "elf-hello",
    .version = "0.1.0",
    .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
    .entry = elf_entry,
    .cleanup = elf_cleanup,
};
