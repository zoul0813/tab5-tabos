#include <tabos/internal/elf_application.h>

#include <tabos/elf_api.h>
#include <tabos/filesystem.h>
#include <tabos/internal/application.h>
#include <tabos/internal/elf_loader.h>
#include <tabos/platform/platform.h>

#include <stdio.h>
#include <stdatomic.h>
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
    atomic_bool exit_requested;
    atomic_int requested_exit_status;
    atomic_bool exec_requested;
    atomic_bool exec_in_flight;
    atomic_bool exec_status_ready;
    atomic_int exec_status;
    char exec_path[TABOS_FS_PATH_MAX];
};

static loader_elf_application_t *application_from_context(tabos_app_context_t *context)
{
    return context != NULL ? context->application_data : NULL;
}

static void elf_console_write(const char *text)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || application->console == NULL || text == NULL) return;
    const char *readable_text = platform_executable_data_pointer(text);
    (void)tabos_console_write_line(application->console, readable_text);
}

static void elf_console_write_raw(const char *text)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || application->console == NULL || text == NULL) return;
    const char *readable_text = platform_executable_data_pointer(text);
    (void)tabos_console_write(application->console, readable_text);
}

static void elf_request_exit(int exit_status)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || application->context == NULL) return;
    atomic_store_explicit(&application->requested_exit_status, exit_status, memory_order_release);
    atomic_store_explicit(&application->exit_requested, true, memory_order_release);
}

static int elf_console_read(char *buffer, uint32_t capacity)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || application->console == NULL || buffer == NULL || capacity < 2U) {
        return -1;
    }
    tabos_input_event_t event;
    while (tabos_console_poll(application->console, &event)) {
        if (event.type == TABOS_INPUT_TEXT) {
            const size_t length = strlen(event.text);
            const size_t copied = length < (size_t)capacity - 1U
                ? length : (size_t)capacity - 1U;
            memcpy(buffer, event.text, copied);
            buffer[copied] = '\0';
            return (int)copied;
        }
        if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_BACKSPACE) {
            buffer[0] = '\b';
            buffer[1] = '\0';
            return 1;
        }
    }
    buffer[0] = '\0';
    return 0;
}

static int elf_console_clear(void)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    return application != NULL && application->console != NULL &&
        tabos_console_clear(application->console) ? 0 : -1;
}

static int elf_fs_getcwd(char *buffer, uint32_t capacity)
{
    return tabos_fs_getcwd(buffer, capacity) != NULL ? 0 : -*tabos_errno_location();
}

static int elf_fs_chdir(const char *path)
{
    return tabos_fs_chdir(path) == 0 ? 0 : -*tabos_errno_location();
}

static int elf_fs_list(const char *path, char *buffer, uint32_t capacity)
{
    if (path == NULL || buffer == NULL || capacity == 0U) return -TABOS_EINVAL;
    const tabos_dir_t directory = tabos_fs_opendir(path);
    if (directory < 0) return -*tabos_errno_location();
    size_t used = 0U;
    tabos_dirent_t entry;
    int result = 0;
    while ((result = tabos_fs_readdir(directory, &entry)) > 0) {
        const size_t length = strlen(entry.name);
        if (length + 1U >= (size_t)capacity - used) {
            result = -TABOS_ENOSPC;
            break;
        }
        memcpy(buffer + used, entry.name, length);
        used += length;
        buffer[used++] = '\n';
    }
    if (result < 0) result = -*tabos_errno_location();
    if (tabos_fs_closedir(directory) != 0 && result == 0) result = -*tabos_errno_location();
    buffer[used] = '\0';
    return result;
}

static int elf_exec(const char *path)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || path == NULL || path[0] == '\0') return -TABOS_EINVAL;
    if (atomic_exchange_explicit(&application->exec_status_ready, false,
                                 memory_order_acq_rel)) {
        const int status = atomic_load_explicit(&application->exec_status,
                                                memory_order_acquire);
        atomic_store_explicit(&application->exec_in_flight, false, memory_order_release);
        return status;
    }
    if (atomic_load_explicit(&application->exec_in_flight, memory_order_acquire)) {
        return TABOS_ELF_EXEC_PENDING;
    }
    const size_t length = strlen(path);
    if (length >= sizeof(application->exec_path)) return -TABOS_ENAMETOOLONG;
    memcpy(application->exec_path, path, length + 1U);
    atomic_store_explicit(&application->exec_in_flight, true, memory_order_release);
    atomic_store_explicit(&application->exec_requested, true, memory_order_release);
    return TABOS_ELF_EXEC_PENDING;
}

static void elf_yield(void)
{
    platform_input_wait();
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
        .console_read = elf_console_read,
        .console_clear = elf_console_clear,
        .fs_getcwd = elf_fs_getcwd,
        .fs_chdir = elf_fs_chdir,
        .fs_list = elf_fs_list,
        .exec = elf_exec,
        .yield = elf_yield,
        .console_write_raw = elf_console_write_raw,
    };
    application->execution = platform_riscv32_create(
        application->image.entry, application->image.memory, application->image.memory_size,
        application->image.info.minimum_address, &api, application);
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

    int child_status = 0;
    if (tabos_app_take_child_status(context, &child_status)) {
        atomic_store_explicit(&application->exec_status, child_status, memory_order_release);
        atomic_store_explicit(&application->exec_status_ready, true, memory_order_release);
    }
    if (atomic_exchange_explicit(&application->exec_requested, false, memory_order_acq_rel)) {
        const tabos_app_result_t launch_result = tabos_app_exec(context, application->exec_path);
        if (launch_result != TABOS_APP_RESULT_OK) {
            atomic_store_explicit(&application->exec_status, -(100 + (int)launch_result),
                                  memory_order_release);
            atomic_store_explicit(&application->exec_status_ready, true, memory_order_release);
        } else {
            return;
        }
    }
    if (atomic_exchange_explicit(&application->exit_requested, false, memory_order_acq_rel)) {
        tabos_app_request_exit(
            context, atomic_load_explicit(&application->requested_exit_status,
                                          memory_order_acquire));
        return;
    }

    int returned_status = 0;
    const platform_riscv32_result_t result = platform_riscv32_step(
        application->execution, ELF_INSTRUCTIONS_PER_UPDATE, &returned_status);
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
    if (!atomic_load_explicit(&application->exit_requested, memory_order_acquire)) {
        tabos_app_request_exit(context, returned_status);
    }
}

static void elf_cleanup(tabos_app_context_t *context, int exit_status)
{
    (void)exit_status;
    loader_elf_application_t *application = application_from_context(context);
    if (application == NULL) return;
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
