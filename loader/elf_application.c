#include <tabos/internal/elf_application.h>

#include <tabos/internal/elf_api.h>
#include <tabos/filesystem.h>
#include <tabos/internal/application.h>
#include <tabos/internal/elf_loader.h>
#include <tabos/internal/filesystem.h>
#include <tabos/internal/display.h>
#include <tabos/internal/console.h>
#include <tabos/internal/raster.h>
#include <tabos/platform/platform.h>
#include <tabos/config/display.h>
#include <tabos/tty.h>

#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

enum {
    /* The P4 backend runs ELF code natively and ignores this interpreter budget.
       Keep host slices large enough for framebuffer workloads without starving its
       event loop for an unbounded period. */
    ELF_INSTRUCTIONS_PER_UPDATE = 2000000,
    ELF_DESCRIPTOR_CAPACITY = 16,
    ELF_HEAP_MAX = 1024 * 1024,
    ELF_GRAPHICS_COMMAND_CAPACITY = 64,
};

typedef enum {
    ELF_GRAPHICS_FILL,
    ELF_GRAPHICS_BLIT,
} elf_graphics_command_type_t;

typedef struct {
    elf_graphics_command_type_t type;
    union {
        struct {
            int32_t x;
            int32_t y;
            uint32_t width;
            uint32_t height;
            tabos_color_t color;
        } fill;
        tabos_graphics_blit_options_t blit;
    } data;
} elf_graphics_command_t;

typedef struct {
    tabos_fd_t kernel_descriptor;
    int flags;
    bool open;
} elf_descriptor_t;

struct loader_elf_application {
    tabos_app_descriptor_t descriptor;
    char name[TABOS_FS_NAME_MAX + 1U];
    char path[TABOS_FS_PATH_MAX];
    size_t argc;
    char argument_data[TABOS_ELF_ARG_BYTES_MAX];
    const char *argv[TABOS_ELF_ARG_MAX + 1U];
    tabos_app_context_t *context;
    const tabos_console_session_t *console;
    loader_elf_image_t image;
    platform_riscv32_context_t *execution;
    atomic_bool exit_requested;
    atomic_int requested_exit_status;
    atomic_bool exec_requested;
    atomic_bool exec_in_flight;
    atomic_bool exec_status_ready;
    bool graphics_active;
    elf_graphics_command_t graphics_commands[ELF_GRAPHICS_COMMAND_CAPACITY];
    size_t graphics_command_head;
    size_t graphics_command_count;
    atomic_int exec_status;
    char exec_path[TABOS_FS_PATH_MAX];
    size_t exec_argc;
    char exec_argument_data[TABOS_ELF_ARG_BYTES_MAX];
    const char *exec_argv[TABOS_ELF_ARG_MAX + 1U];
    elf_descriptor_t descriptors[ELF_DESCRIPTOR_CAPACITY];
    char working_directory[TABOS_FS_PATH_MAX];
    uint8_t *heap;
    size_t heap_used;
    uint32_t tty_mode;
    char input_pending[4];
    uint8_t input_pending_offset;
    uint8_t input_pending_length;
};

static bool elf_handle_tty_navigation(loader_elf_application_t *application,
                                      const tabos_input_event_t *event)
{
    if (application == NULL || event == NULL ||
        application->graphics_active ||
        (application->tty_mode & TABOS_TTY_MODE_SCROLL_KEYS) == 0U ||
        event->type != TABOS_INPUT_KEY_DOWN) return false;
    if (event->key == TABOS_KEY_PAGE_UP ||
        (event->key == TABOS_KEY_UP &&
         (event->modifiers & TABOS_MODIFIER_CONTROL) != 0U)) {
        (void)tabos_console_page_up(application->console); return true;
    }
    if (event->key == TABOS_KEY_PAGE_DOWN ||
        (event->key == TABOS_KEY_DOWN &&
         (event->modifiers & TABOS_MODIFIER_CONTROL) != 0U)) {
        (void)tabos_console_page_down(application->console); return true;
    }
    if (event->key == TABOS_KEY_HOME ||
        (event->key == TABOS_KEY_LEFT &&
         (event->modifiers & TABOS_MODIFIER_CONTROL) != 0U)) {
        (void)tabos_console_scroll_to_start(application->console); return true;
    }
    if (event->key == TABOS_KEY_END ||
        (event->key == TABOS_KEY_RIGHT &&
         (event->modifiers & TABOS_MODIFIER_CONTROL) != 0U)) {
        (void)tabos_console_scroll_to_end(application->console); return true;
    }
    return false;
}

static bool copy_arguments(size_t argc,
                           const char *const *argv,
                           char *data,
                           size_t data_size,
                           const char **copied_argv)
{
    if (argc > TABOS_ELF_ARG_MAX || (argc > 0U && argv == NULL)) return false;
    argv = platform_executable_data_pointer(argv, (argc + 1U) * sizeof(*argv));
    if (argv == NULL) return false;
    size_t used = 0U;
    for (size_t index = 0U; index < argc; ++index) {
        if (argv[index] == NULL) return false;
        const char *argument = platform_executable_data_pointer(argv[index], data_size - used);
        if (argument == NULL) return false;
        const size_t length = strlen(argument) + 1U;
        if (length > data_size - used) return false;
        copied_argv[index] = data + used;
        memcpy(data + used, argument, length);
        used += length;
    }
    copied_argv[argc] = NULL;
    return true;
}

static loader_elf_application_t *application_from_context(tabos_app_context_t *context)
{
    return context != NULL ? context->application_data : NULL;
}

static void elf_console_write(const char *text)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || application->console == NULL || text == NULL) return;
    const char *readable_text = platform_executable_data_pointer(text, TABOS_ELF_ARG_BYTES_MAX);
    if (readable_text == NULL) return;
    (void)tabos_console_write_line(application->console, readable_text);
}

static void elf_console_write_raw(const char *text)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || application->console == NULL || text == NULL) return;
    const char *readable_text = platform_executable_data_pointer(text, TABOS_ELF_ARG_BYTES_MAX);
    if (readable_text == NULL) return;
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
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || buffer == NULL || capacity == 0U) return -TABOS_EINVAL;
    const size_t needed = strlen(application->working_directory) + 1U;
    if (needed > capacity) return -TABOS_ENAMETOOLONG;
    memcpy(buffer, application->working_directory, needed);
    return 0;
}

static int elf_fs_chdir(const char *path)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || path == NULL) return -TABOS_EINVAL;
    char resolved[TABOS_FS_PATH_MAX];
    const char *readable = platform_executable_data_pointer(path, TABOS_FS_PATH_MAX);
    if (readable == NULL) return -TABOS_EIO;
    if (!filesystem_normalize_path(readable, application->working_directory,
                                   resolved, sizeof(resolved))) return -TABOS_EINVAL;
    tabos_stat_t status;
    if (tabos_fs_stat(resolved, &status) != 0) return -*tabos_errno_location();
    if ((status.mode & TABOS_S_IFDIR) == 0U) return -TABOS_ENOTDIR;
    memcpy(application->working_directory, resolved, strlen(resolved) + 1U);
    return 0;
}

static bool elf_resolve_path(loader_elf_application_t *application, const char *path,
                             char resolved[TABOS_FS_PATH_MAX])
{
    if (application == NULL || path == NULL) return false;
    const char *readable = platform_executable_data_pointer(path, TABOS_FS_PATH_MAX);
    return readable != NULL && filesystem_normalize_path(
        readable, application->working_directory, resolved, TABOS_FS_PATH_MAX);
}

static int elf_fd_open(const char *path, int flags, uint32_t mode)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    char resolved[TABOS_FS_PATH_MAX];
    if (!elf_resolve_path(application, path, resolved)) return -TABOS_EINVAL;
    size_t index = 3U;
    while (index < ELF_DESCRIPTOR_CAPACITY && application->descriptors[index].open) ++index;
    if (index == ELF_DESCRIPTOR_CAPACITY) return -TABOS_EMFILE;
    const tabos_fd_t kernel_descriptor = tabos_fs_open(resolved, flags & ~TABOS_O_NONBLOCK, mode);
    if (kernel_descriptor < 0) return -*tabos_errno_location();
    application->descriptors[index] = (elf_descriptor_t){
        .kernel_descriptor = kernel_descriptor, .flags = flags, .open = true,
    };
    return (int)index;
}

static int elf_fd_close(int descriptor)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || descriptor < 3 || descriptor >= ELF_DESCRIPTOR_CAPACITY ||
        !application->descriptors[descriptor].open) return -TABOS_EBADF;
    const int result = tabos_fs_close(application->descriptors[descriptor].kernel_descriptor);
    if (result != 0) return -*tabos_errno_location();
    application->descriptors[descriptor] = (elf_descriptor_t){0};
    return 0;
}

static int elf_copy_pending_input(loader_elf_application_t *application,
                                  void *buffer, uint32_t count)
{
    const uint32_t available = (uint32_t)application->input_pending_length -
        application->input_pending_offset;
    const uint32_t copied = available < count ? available : count;
    if (copied == 0U) return 0;
    memcpy(buffer, application->input_pending + application->input_pending_offset, copied);
    application->input_pending_offset = (uint8_t)(application->input_pending_offset + copied);
    if (application->input_pending_offset == application->input_pending_length) {
        application->input_pending_offset = 0U;
        application->input_pending_length = 0U;
    }
    return (int)copied;
}

static bool elf_queue_arrow_input(loader_elf_application_t *application,
                                  const tabos_input_event_t *event)
{
    if (event->type != TABOS_INPUT_KEY_DOWN) return false;
    char final = '\0';
    switch (event->key) {
        case TABOS_KEY_UP: final = 'A'; break;
        case TABOS_KEY_DOWN: final = 'B'; break;
        case TABOS_KEY_RIGHT: final = 'C'; break;
        case TABOS_KEY_LEFT: final = 'D'; break;
        default: return false;
    }
    application->input_pending[0] = '\x1b';
    application->input_pending[1] = '[';
    application->input_pending[2] = final;
    application->input_pending_offset = 0U;
    application->input_pending_length = 3U;
    return true;
}

static int elf_input_poll(tabos_input_event_t *event)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || application->console == NULL || event == NULL) {
        return -TABOS_EINVAL;
    }
    while (tabos_console_poll(application->console, event)) {
        if (!elf_handle_tty_navigation(application, event)) return 1;
    }
    return 0;
}

static int elf_fd_read(int descriptor, void *buffer, uint32_t count)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || (buffer == NULL && count != 0U)) return -TABOS_EINVAL;
    if (descriptor == 0) {
        if (count == 0U) return 0;
        const int pending = elf_copy_pending_input(application, buffer, count);
        if (pending != 0) return pending;
        tabos_input_event_t event;
        while (tabos_console_poll(application->console, &event)) {
            if (elf_handle_tty_navigation(application, &event)) continue;
            if (event.type == TABOS_INPUT_TEXT) {
                const size_t length = strlen(event.text);
                const size_t copied = length < count ? length : count;
                memcpy(buffer, event.text, copied);
                return (int)copied;
            }
            if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_BACKSPACE) {
                ((char *)buffer)[0] = '\b';
                return 1;
            }
            if (elf_queue_arrow_input(application, &event)) {
                return elf_copy_pending_input(application, buffer, count);
            }
        }
        return -TABOS_EAGAIN;
    }
    if (descriptor < 3 || descriptor >= ELF_DESCRIPTOR_CAPACITY ||
        !application->descriptors[descriptor].open) return -TABOS_EBADF;
    const tabos_ssize_t result = tabos_fs_read(
        application->descriptors[descriptor].kernel_descriptor, buffer, count);
    return result >= 0 ? (int)result : -*tabos_errno_location();
}

static int elf_tty_get_mode(int descriptor)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || descriptor < 0 || descriptor > 2) return -TABOS_ENOTTY;
    return (int)application->tty_mode;
}

static int elf_tty_set_mode(int descriptor, uint32_t mode)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || descriptor < 0 || descriptor > 2) return -TABOS_ENOTTY;
    if ((mode & ~(uint32_t)TABOS_TTY_MODE_SCROLL_KEYS) != 0U) return -TABOS_EINVAL;
    application->tty_mode = mode;
    return 0;
}

static int elf_fd_write(int descriptor, const void *buffer, uint32_t count)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || (buffer == NULL && count != 0U)) return -TABOS_EINVAL;
    if (descriptor == 1 || descriptor == 2) {
        const char *source = platform_executable_data_pointer(buffer, count);
        if (source == NULL) return -TABOS_EIO;
        char chunk[129];
        uint32_t written = 0U;
        while (written < count) {
            const uint32_t length = count - written < sizeof(chunk) - 1U
                ? count - written : (uint32_t)sizeof(chunk) - 1U;
            memcpy(chunk, source + written, length);
            chunk[length] = '\0';
            if (!tabos_console_write(application->console, chunk)) return -TABOS_EIO;
            written += length;
        }
        return (int)count;
    }
    if (descriptor < 3 || descriptor >= ELF_DESCRIPTOR_CAPACITY ||
        !application->descriptors[descriptor].open) return -TABOS_EBADF;
    const void *source = platform_executable_data_pointer(buffer, count);
    if (source == NULL) return -TABOS_EIO;
    const tabos_ssize_t result = tabos_fs_write(
        application->descriptors[descriptor].kernel_descriptor, source, count);
    return result >= 0 ? (int)result : -*tabos_errno_location();
}

static int elf_fd_seek(int descriptor, int32_t offset, int whence, int32_t *position)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || position == NULL || descriptor < 3 ||
        descriptor >= ELF_DESCRIPTOR_CAPACITY || !application->descriptors[descriptor].open) {
        return -TABOS_EBADF;
    }
    const tabos_off_t result = tabos_fs_seek(
        application->descriptors[descriptor].kernel_descriptor, offset, whence);
    if (result < 0) return -*tabos_errno_location();
    if (result > INT32_MAX) return -TABOS_ENOTSUP;
    *position = (int32_t)result;
    return 0;
}

static void elf_copy_stat(tabos_elf_stat_t *destination, const tabos_stat_t *source)
{
    destination->mode = source->mode;
    destination->size_low = (uint32_t)source->size;
    destination->size_high = (uint32_t)(source->size >> 32U);
    destination->modified_time_low = (int32_t)source->modified_time;
    destination->modified_time_high = (int32_t)(source->modified_time >> 32U);
}

static int elf_fs_stat_path(const char *path, tabos_elf_stat_t *status)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    char resolved[TABOS_FS_PATH_MAX];
    tabos_stat_t native_status;
    if (status == NULL || !elf_resolve_path(application, path, resolved)) return -TABOS_EINVAL;
    if (tabos_fs_stat(resolved, &native_status) != 0) return -*tabos_errno_location();
    elf_copy_stat(status, &native_status);
    return 0;
}

static int elf_fd_stat(int descriptor, tabos_elf_stat_t *status)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (status == NULL || application == NULL) return -TABOS_EINVAL;
    if (descriptor >= 0 && descriptor <= 2) {
        *status = (tabos_elf_stat_t){0};
        return 0;
    }
    if (descriptor < 3 || descriptor >= ELF_DESCRIPTOR_CAPACITY ||
        !application->descriptors[descriptor].open) return -TABOS_EBADF;
    tabos_stat_t native_status;
    if (tabos_fs_fstat(application->descriptors[descriptor].kernel_descriptor,
                       &native_status) != 0) return -*tabos_errno_location();
    elf_copy_stat(status, &native_status);
    return 0;
}

static int elf_fs_mkdir_path(const char *path, uint32_t mode)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    char resolved[TABOS_FS_PATH_MAX];
    if (!elf_resolve_path(application, path, resolved)) return -TABOS_EINVAL;
    return tabos_fs_mkdir(resolved, mode) == 0 ? 0 : -*tabos_errno_location();
}

static int elf_fs_unlink_path(const char *path)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    char resolved[TABOS_FS_PATH_MAX];
    if (!elf_resolve_path(application, path, resolved)) return -TABOS_EINVAL;
    return tabos_fs_unlink(resolved) == 0 ? 0 : -*tabos_errno_location();
}

static int elf_fs_rmdir_path(const char *path)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    char resolved[TABOS_FS_PATH_MAX];
    if (!elf_resolve_path(application, path, resolved)) return -TABOS_EINVAL;
    return tabos_fs_rmdir(resolved) == 0 ? 0 : -*tabos_errno_location();
}

static int elf_fs_rename_path(const char *old_path, const char *new_path)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    char old_resolved[TABOS_FS_PATH_MAX];
    char new_resolved[TABOS_FS_PATH_MAX];
    if (!elf_resolve_path(application, old_path, old_resolved) ||
        !elf_resolve_path(application, new_path, new_resolved)) return -TABOS_EINVAL;
    return tabos_fs_rename(old_resolved, new_resolved) == 0
        ? 0 : -*tabos_errno_location();
}

static int elf_fd_get_flags(int descriptor)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || descriptor < 0 || descriptor >= ELF_DESCRIPTOR_CAPACITY ||
        !application->descriptors[descriptor].open) return -TABOS_EBADF;
    return application->descriptors[descriptor].flags;
}

static int elf_fd_set_flags(int descriptor, int flags)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || descriptor < 0 || descriptor >= ELF_DESCRIPTOR_CAPACITY ||
        !application->descriptors[descriptor].open) return -TABOS_EBADF;
    application->descriptors[descriptor].flags =
        (application->descriptors[descriptor].flags & ~TABOS_O_NONBLOCK) |
        (flags & TABOS_O_NONBLOCK);
    return 0;
}

static void *elf_heap_sbrk(int32_t increment)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || increment < 0 ||
        (size_t)increment > ELF_HEAP_MAX - application->heap_used) return (void *)-1;
    if (application->heap == NULL) {
        application->heap = malloc(ELF_HEAP_MAX);
        if (application->heap == NULL) return (void *)-1;
    }
    void *previous = application->heap + application->heap_used;
    application->heap_used += (size_t)increment;
    return previous;
}

static int elf_fs_list(const char *path, char *buffer, uint32_t capacity)
{
    if (path == NULL || buffer == NULL || capacity == 0U) return -TABOS_EINVAL;
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL) return -TABOS_EINVAL;
    char resolved[TABOS_FS_PATH_MAX];
    if (!filesystem_normalize_path(path, application->working_directory,
                                   resolved, sizeof(resolved))) {
        return -TABOS_ENAMETOOLONG;
    }
    const tabos_dir_t directory = tabos_fs_opendir(resolved);
    if (directory < 0) return -*tabos_errno_location();
    size_t used = 0U;
    tabos_dirent_t entry;
    int result = 0;
    while ((result = tabos_fs_readdir(directory, &entry)) > 0) {
        const size_t length = strlen(entry.name);
        if (length + 3U >= (size_t)capacity - used) {
            result = -TABOS_ENOSPC;
            break;
        }
        buffer[used++] = (entry.mode & TABOS_S_IFDIR) != 0U ? 'D' : 'F';
        buffer[used++] = ':';
        memcpy(buffer + used, entry.name, length);
        used += length;
        buffer[used++] = '\n';
    }
    if (result < 0) result = -*tabos_errno_location();
    if (tabos_fs_closedir(directory) != 0 && result == 0) result = -*tabos_errno_location();
    buffer[used] = '\0';
    return result;
}

static uint64_t elf_monotonic_ms(void)
{
    return platform_time_ms();
}

static int elf_system_info(tabos_elf_system_info_t *info)
{
    if (info == NULL) return -TABOS_EINVAL;
    platform_diagnostics_t diagnostics;
    if (!platform_get_diagnostics(&diagnostics)) return -TABOS_EIO;
    memset(info, 0, sizeof(*info));
    (void)snprintf(info->target, sizeof(info->target), "%s", platform_name());
    (void)snprintf(info->device, sizeof(info->device), "%s",
                   diagnostics.device_name != NULL ? diagnostics.device_name : "Unknown");
    (void)snprintf(info->display, sizeof(info->display), "%s", platform_display_name());
    info->display_width = TABOS_DISPLAY_WIDTH;
    info->display_height = TABOS_DISPLAY_HEIGHT;
    info->cpu_cores = diagnostics.cpu_cores;
    info->cpu_frequency_mhz = diagnostics.cpu_frequency_mhz;
    info->memory_total_low = (uint32_t)diagnostics.memory_total_bytes;
    info->memory_total_high = (uint32_t)(diagnostics.memory_total_bytes >> 32U);
    info->external_memory_total_low = (uint32_t)diagnostics.external_memory_total_bytes;
    info->external_memory_total_high =
        (uint32_t)(diagnostics.external_memory_total_bytes >> 32U);
    return 0;
}

static int elf_graphics_open(uint32_t *width, uint32_t *height)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    platform_framebuffer_t *framebuffer = display_framebuffer();
    if (application == NULL || framebuffer == NULL || width == NULL || height == NULL) return -TABOS_EINVAL;
    if (!platform_graphics_begin()) return -TABOS_EIO;
    application->graphics_active = true;
    console_set_graphics_active(true);
    *width = (uint32_t)framebuffer->width;
    *height = (uint32_t)framebuffer->height;
    return 0;
}

static bool elf_graphics_execute_one(loader_elf_application_t *application)
{
    if (application == NULL || application->graphics_command_count == 0U) return true;
    platform_framebuffer_t *framebuffer = display_framebuffer();
    if (framebuffer == NULL) return false;
    elf_graphics_command_t *command =
        &application->graphics_commands[application->graphics_command_head];
    bool result = true;
    if (command->type == ELF_GRAPHICS_FILL) {
        if (!platform_graphics_fill(framebuffer, command->data.fill.x,
                                    command->data.fill.y, command->data.fill.width,
                                    command->data.fill.height,
                                    command->data.fill.color)) {
            raster_fill(framebuffer, command->data.fill.x, command->data.fill.y,
                        command->data.fill.width, command->data.fill.height,
                        command->data.fill.color);
        }
    } else {
        result = platform_graphics_blit(framebuffer, &command->data.blit) ||
            raster_blit(framebuffer, &command->data.blit);
    }
    application->graphics_command_head =
        (application->graphics_command_head + 1U) % ELF_GRAPHICS_COMMAND_CAPACITY;
    --application->graphics_command_count;
    return result;
}

static bool elf_graphics_drain(loader_elf_application_t *application)
{
    bool result = true;
    while (application != NULL && application->graphics_command_count != 0U) {
        if (!elf_graphics_execute_one(application)) result = false;
    }
    return result;
}

static bool elf_graphics_enqueue(loader_elf_application_t *application,
                                 const elf_graphics_command_t *command)
{
    if (application == NULL || command == NULL) return false;
    if (application->graphics_command_count == ELF_GRAPHICS_COMMAND_CAPACITY &&
        !elf_graphics_execute_one(application)) return false;
    const size_t tail = (application->graphics_command_head +
                         application->graphics_command_count) %
        ELF_GRAPHICS_COMMAND_CAPACITY;
    application->graphics_commands[tail] = *command;
    ++application->graphics_command_count;
    return true;
}

static int elf_graphics_clear(uint32_t color)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    platform_framebuffer_t *framebuffer = display_framebuffer();
    if (application == NULL || !application->graphics_active || framebuffer == NULL) return -TABOS_EINVAL;
    const elf_graphics_command_t command = {
        .type = ELF_GRAPHICS_FILL,
        .data.fill = {.x = 0, .y = 0, .width = (uint32_t)framebuffer->width,
                      .height = (uint32_t)framebuffer->height,
                      .color = (tabos_color_t)color},
    };
    return elf_graphics_enqueue(application, &command) ? 0 : -TABOS_EIO;
}

static int elf_graphics_fill_rect(int32_t x, int32_t y, uint32_t width, uint32_t height,
                                  uint32_t color)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    platform_framebuffer_t *framebuffer = display_framebuffer();
    if (application == NULL || !application->graphics_active || framebuffer == NULL) return -TABOS_EINVAL;
    const elf_graphics_command_t command = {
        .type = ELF_GRAPHICS_FILL,
        .data.fill = {.x = x, .y = y, .width = width, .height = height,
                      .color = (tabos_color_t)color},
    };
    return elf_graphics_enqueue(application, &command) ? 0 : -TABOS_EIO;
}

static int elf_graphics_blit(int32_t x, int32_t y, uint32_t width, uint32_t height,
                             const uint16_t *pixels)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    platform_framebuffer_t *framebuffer = display_framebuffer();
    const size_t pixel_bytes = width <= SIZE_MAX / height &&
        (size_t)width * height <= SIZE_MAX / sizeof(*pixels)
        ? (size_t)width * height * sizeof(*pixels) : 0U;
    const uint16_t *source = platform_executable_data_pointer(pixels, pixel_bytes);
    if (application == NULL || !application->graphics_active || framebuffer == NULL || source == NULL ||
        width == 0U || height == 0U || width > SIZE_MAX / height) return -TABOS_EINVAL;
    const elf_graphics_command_t command = {
        .type = ELF_GRAPHICS_BLIT,
        .data.blit = {
            .pixels = source, .bitmap_width = width, .bitmap_height = height,
            .source = {.x = 0, .y = 0, .width = width, .height = height},
            .destination = {.x = x, .y = y, .width = width, .height = height},
            .rotation = TABOS_GRAPHICS_ROTATE_0, .opacity = 255U,
        },
    };
    return elf_graphics_enqueue(application, &command) ? 0 : -TABOS_EIO;
}

static uint32_t elf_graphics_capabilities(void)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || !application->graphics_active) return 0U;
    return TABOS_GRAPHICS_CAP_QUEUED | TABOS_GRAPHICS_CAP_TRANSFORM |
        TABOS_GRAPHICS_CAP_BLEND | TABOS_GRAPHICS_CAP_COLOR_KEY |
        platform_graphics_capabilities();
}

static int elf_graphics_blit_ex(const tabos_graphics_blit_options_t *options)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    const tabos_graphics_blit_options_t *readable =
        platform_executable_data_pointer(options, sizeof(*options));
    if (application == NULL || !application->graphics_active || readable == NULL ||
        readable->bitmap_width == 0U || readable->bitmap_height == 0U ||
        readable->bitmap_width > SIZE_MAX / readable->bitmap_height ||
        (size_t)readable->bitmap_width * readable->bitmap_height >
            SIZE_MAX / sizeof(*readable->pixels)) return -TABOS_EINVAL;
    tabos_graphics_blit_options_t copied = *readable;
    const size_t bytes = (size_t)copied.bitmap_width * copied.bitmap_height *
        sizeof(*copied.pixels);
    copied.pixels = platform_executable_data_pointer(copied.pixels, bytes);
    if (copied.pixels == NULL) return -TABOS_EINVAL;
    const elf_graphics_command_t command = {
        .type = ELF_GRAPHICS_BLIT, .data.blit = copied,
    };
    return elf_graphics_enqueue(application, &command) ? 0 : -TABOS_EIO;
}

static int elf_graphics_present(void)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    return application != NULL && application->graphics_active &&
        elf_graphics_drain(application) && platform_graphics_present(display_framebuffer())
            ? 0 : -TABOS_EIO;
}

static int elf_graphics_close(void)
{
    loader_elf_application_t *application = platform_riscv32_current_user_data();
    if (application == NULL || !application->graphics_active) return -TABOS_EINVAL;
    if (!elf_graphics_drain(application) ||
        !platform_graphics_present(display_framebuffer())) return -TABOS_EIO;
    application->graphics_active = false;
    platform_graphics_end();
    console_set_graphics_active(false);
    return 0;
}

static int elf_exec(const char *path, uint32_t argc, const char *const *argv)
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
    const char *readable_path = platform_executable_data_pointer(path, TABOS_FS_PATH_MAX);
    if (readable_path == NULL) return -TABOS_EIO;
    const size_t length = strlen(readable_path);
    if (length >= sizeof(application->exec_path)) return -TABOS_ENAMETOOLONG;
    if (!copy_arguments((size_t)argc, argv,
                        application->exec_argument_data,
                        sizeof(application->exec_argument_data),
                        application->exec_argv)) {
        return -TABOS_EINVAL;
    }
    memcpy(application->exec_path, readable_path, length + 1U);
    application->exec_argc = (size_t)argc;
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
        .fd_open = elf_fd_open,
        .fd_close = elf_fd_close,
        .fd_read = elf_fd_read,
        .fd_write = elf_fd_write,
        .fd_seek = elf_fd_seek,
        .fs_stat = elf_fs_stat_path,
        .fd_stat = elf_fd_stat,
        .fs_mkdir = elf_fs_mkdir_path,
        .fs_unlink = elf_fs_unlink_path,
        .fs_rename = elf_fs_rename_path,
        .fd_get_flags = elf_fd_get_flags,
        .fd_set_flags = elf_fd_set_flags,
        .heap_sbrk = elf_heap_sbrk,
        .fs_rmdir = elf_fs_rmdir_path,
        .monotonic_ms = elf_monotonic_ms,
        .system_info = elf_system_info,
        .graphics_open = elf_graphics_open,
        .graphics_clear = elf_graphics_clear,
        .graphics_fill_rect = elf_graphics_fill_rect,
        .graphics_blit = elf_graphics_blit,
        .graphics_present = elf_graphics_present,
        .graphics_close = elf_graphics_close,
        .graphics_capabilities = elf_graphics_capabilities,
        .graphics_blit_ex = elf_graphics_blit_ex,
        .tty_get_mode = elf_tty_get_mode,
        .tty_set_mode = elf_tty_set_mode,
        .input_poll = elf_input_poll,
    };
    application->execution = platform_riscv32_create(
        application->image.entry, application->image.memory, application->image.memory_size,
        application->image.info.minimum_address, &api,
        application->argc, application->argv, application);
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
        kernel_process_fail(context, TABOS_PROCESS_TERMINATION_FAULT, 5);
        return;
    }

    int child_status = 0;
    if (tabos_app_take_child_status(context, &child_status)) {
        atomic_store_explicit(&application->exec_status, child_status, memory_order_release);
        atomic_store_explicit(&application->exec_status_ready, true, memory_order_release);
    }
    if (atomic_exchange_explicit(&application->exec_requested, false, memory_order_acq_rel)) {
        const tabos_app_result_t launch_result = tabos_app_exec_args(
            context, application->exec_path, application->exec_argc, application->exec_argv);
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
        kernel_process_fail(context, TABOS_PROCESS_TERMINATION_FAULT, 5);
        return;
    }
    char exit_message[56];
    (void)snprintf(exit_message, sizeof(exit_message),
                   "ELF entry returned status %d", returned_status);
    platform_log(exit_message);
    if (!atomic_load_explicit(&application->exit_requested, memory_order_acquire)) {
        kernel_process_fail(context, TABOS_PROCESS_TERMINATION_RETURN, returned_status);
    }
}

static void elf_cleanup(tabos_app_context_t *context, int exit_status)
{
    (void)exit_status;
    loader_elf_application_t *application = application_from_context(context);
    if (application == NULL) return;
    if (application->graphics_active) {
        (void)elf_graphics_drain(application);
        (void)platform_graphics_present(display_framebuffer());
        application->graphics_active = false;
        platform_graphics_end();
        console_set_graphics_active(false);
    }
    platform_riscv32_destroy(application->execution);
    application->execution = NULL;
    loader_elf_unload(&application->image);
    for (size_t index = 3U; index < ELF_DESCRIPTOR_CAPACITY; ++index) {
        if (application->descriptors[index].open) {
            (void)tabos_fs_close(application->descriptors[index].kernel_descriptor);
            application->descriptors[index] = (elf_descriptor_t){0};
        }
    }
    free(application->heap);
    application->heap = NULL;
    application->heap_used = 0U;
    application->context = NULL;
    application->console = NULL;
}

loader_elf_application_t *loader_elf_application_create(const char *path,
                                                        size_t argc,
                                                        const char *const *argv)
{
    if (path == NULL || path[0] == '\0') return NULL;
    const size_t path_length = strlen(path);
    if (path_length >= TABOS_FS_PATH_MAX) return NULL;

    loader_elf_application_t *application = calloc(1U, sizeof(*application));
    if (application == NULL) return NULL;
    if (!copy_arguments(argc, argv, application->argument_data,
                        sizeof(application->argument_data), application->argv)) {
        free(application);
        return NULL;
    }
    application->argc = argc;
    memcpy(application->path, path, path_length + 1U);
    for (size_t index = 0U; index < 3U; ++index) {
        application->descriptors[index] = (elf_descriptor_t){.flags = 0, .open = true};
    }
    if (tabos_fs_getcwd(application->working_directory,
                        sizeof(application->working_directory)) == NULL) {
        memcpy(application->working_directory, "A:/", 4U);
    }

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
    loader_elf_application_t *elf_application = application;
    if (elf_application != NULL && elf_application->graphics_active) {
        platform_graphics_end();
        console_set_graphics_active(false);
    }
    free(elf_application);
}

const char *loader_elf_application_working_directory(
    const loader_elf_application_t *application)
{
    return application != NULL ? application->working_directory : NULL;
}

bool loader_elf_application_set_working_directory(loader_elf_application_t *application,
                                                  const char *working_directory)
{
    if (application == NULL || working_directory == NULL ||
        strlen(working_directory) >= sizeof(application->working_directory)) return false;
    memcpy(application->working_directory, working_directory, strlen(working_directory) + 1U);
    return true;
}

uint32_t loader_elf_application_tty_mode(const loader_elf_application_t *application)
{
    return application != NULL ? application->tty_mode : 0U;
}

bool loader_elf_application_set_tty_mode(loader_elf_application_t *application, uint32_t mode)
{
    if (application == NULL ||
        (mode & ~(uint32_t)TABOS_TTY_MODE_SCROLL_KEYS) != 0U) return false;
    application->tty_mode = mode;
    return true;
}
