#include <tabos/internal/elf_application.h>

#include <tabos/elf_api.h>
#include <tabos/filesystem.h>
#include <tabos/internal/application.h>
#include <tabos/internal/elf_loader.h>
#include <tabos/platform/platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ELF_INSTRUCTIONS_PER_UPDATE = 10000 };

struct loader_elf_application {
    tabos_app_descriptor_t descriptor;
    char name[TABOS_FS_NAME_MAX + 1U];
    char path[TABOS_FS_PATH_MAX];
    tabos_app_context_t *context;
    const tabos_console_session_t *console;
    loader_elf_image_t image;
    platform_riscv32_context_t *execution;
    bool exit_requested;
};

static loader_elf_application_t *active_application;

static loader_elf_application_t *application_from_context(tabos_app_context_t *context)
{
    return context != NULL ? context->application_data : NULL;
}

static void elf_console_write(const char *text)
{
    if (active_application == NULL || active_application->console == NULL || text == NULL) return;
    const char *readable_text = platform_executable_data_pointer(text);
    (void)tabos_console_write_line(active_application->console, readable_text);
}

static void elf_request_exit(int exit_status)
{
    if (active_application == NULL || active_application->context == NULL) return;
    active_application->exit_requested = true;
    tabos_app_request_exit(active_application->context, exit_status);
}

static bool elf_entry(tabos_app_context_t *context)
{
    loader_elf_application_t *application = application_from_context(context);
    if (application == NULL) return false;
    application->context = context;
    application->console = tabos_app_console(context);
    if (application->console == NULL) return false;

    const loader_elf_result_t result = loader_elf_load_file(application->path,
                                                            &application->image);
    if (result != LOADER_ELF_OK) {
        char message[96];
        (void)snprintf(message, sizeof(message), "ELF load FAILED: %s",
                       loader_elf_result_name(result));
        platform_log(message);
        (void)tabos_console_write(application->console, "ELF load FAILED: ");
        (void)tabos_console_write_line(application->console, loader_elf_result_name(result));
        return false;
    }
    if (!platform_can_execute_riscv32()) {
        (void)tabos_console_write_line(application->console,
                                       "ELF execution unsupported on this target");
        return false;
    }

    char load_message[640];
    (void)snprintf(load_message, sizeof(load_message),
                   "ELF image loaded from %s: %u memory bytes", application->path,
                   (unsigned int)application->image.memory_size);
    platform_log(load_message);

    const tabos_elf_api_t api = {
        .abi_version = TABOS_ELF_API_VERSION,
        .console_write = elf_console_write,
        .request_exit = elf_request_exit,
    };
    application->execution = platform_riscv32_create(
        application->image.entry, application->image.memory, application->image.memory_size,
        application->image.info.minimum_address, &api);
    if (application->execution == NULL) {
        (void)tabos_console_write_line(application->console, "ELF execution FAILED");
        return false;
    }
    return true;
}

static void elf_update(tabos_app_context_t *context)
{
    loader_elf_application_t *application = application_from_context(context);
    if (application == NULL || application->execution == NULL) {
        tabos_app_request_exit(context, 5);
        return;
    }

    int returned_status = 0;
    active_application = application;
    const platform_riscv32_result_t result = platform_riscv32_step(
        application->execution, ELF_INSTRUCTIONS_PER_UPDATE, &returned_status);
    active_application = NULL;
    if (result == PLATFORM_RISCV32_YIELDED) return;
    if (result == PLATFORM_RISCV32_FAULT) {
        (void)tabos_console_write_line(application->console, "ELF execution FAILED");
        tabos_app_request_exit(context, 5);
        return;
    }
    char exit_message[56];
    (void)snprintf(exit_message, sizeof(exit_message),
                   "ELF entry returned status %d", returned_status);
    platform_log(exit_message);
    if (!application->exit_requested) tabos_app_request_exit(context, returned_status);
}

static void elf_cleanup(tabos_app_context_t *context, int exit_status)
{
    (void)exit_status;
    loader_elf_application_t *application = application_from_context(context);
    if (application == NULL) return;
    if (active_application == application) active_application = NULL;
    platform_riscv32_destroy(application->execution);
    application->execution = NULL;
    loader_elf_unload(&application->image);
    application->context = NULL;
    application->console = NULL;
}

loader_elf_application_t *loader_elf_application_create(const char *path)
{
    if (path == NULL || path[0] == '\0') return NULL;
    const size_t path_length = strlen(path);
    if (path_length >= TABOS_FS_PATH_MAX) return NULL;

    loader_elf_application_t *application = calloc(1U, sizeof(*application));
    if (application == NULL) return NULL;
    memcpy(application->path, path, path_length + 1U);

    const char *name = strrchr(path, '/');
    name = name != NULL ? name + 1 : path;
    if (name[0] == '\0' || strlen(name) > TABOS_FS_NAME_MAX) {
        free(application);
        return NULL;
    }
    memcpy(application->name, name, strlen(name) + 1U);
    application->descriptor = (tabos_app_descriptor_t){
        .abi_version = TABOS_APPLICATION_ABI_VERSION,
        .name = application->name,
        .version = "ELF",
        .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
        .entry = elf_entry,
        .update = elf_update,
        .cleanup = elf_cleanup,
    };
    return application;
}

const tabos_app_descriptor_t *loader_elf_application_descriptor(
    const loader_elf_application_t *application)
{
    return application != NULL ? &application->descriptor : NULL;
}

void loader_elf_application_destroy(void *application)
{
    free(application);
}
