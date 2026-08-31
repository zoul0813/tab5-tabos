#include <tabos/internal/elf_application.h>

#include <tabos/internal/elf_api.h>
#include <tabos/filesystem.h>
#include <tabos/network.h>
#include <tabos/tls.h>
#include <tabos/wait.h>
#include <tabos/internal/application.h>
#include <tabos/internal/elf_loader.h>
#include <tabos/internal/filesystem.h>
#include <tabos/internal/display.h>
#include <tabos/internal/device_registry.h>
#include <tabos/internal/hardware_devices.h>
#include <tabos/internal/console.h>
#include <tabos/internal/raster.h>
#include <tabos/internal/runtime.h>
#include <tabos/internal/network.h>
#include <tabos/internal/network_config.h>
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
    ELF_INSTRUCTIONS_PER_UPDATE    = 2000000,
    ELF_DESCRIPTOR_CAPACITY        = 16,
    ELF_SOCKET_CAPACITY            = TABOS_SOCKET_MAX,
    ELF_SOCKET_INDEX_BITS          = 3,
    ELF_SOCKET_INDEX_MASK          = ELF_SOCKET_CAPACITY - 1,
    ELF_SOCKET_GENERATION_MAX      = INT32_MAX >> ELF_SOCKET_INDEX_BITS,
    ELF_TLS_CAPACITY               = TABOS_TLS_MAX,
    ELF_TLS_INDEX_BITS             = 2,
    ELF_TLS_INDEX_MASK             = ELF_TLS_CAPACITY - 1,
    ELF_TLS_GENERATION_MAX         = INT32_MAX >> ELF_TLS_INDEX_BITS,
    ELF_WAIT_SOURCE_CAPACITY       = TABOS_WAIT_MAX,
    ELF_WAIT_SOURCE_INDEX_BITS     = 4,
    ELF_WAIT_SOURCE_INDEX_MASK     = ELF_WAIT_SOURCE_CAPACITY - 1,
    ELF_WAIT_SOURCE_GENERATION_MAX = INT32_MAX >> ELF_WAIT_SOURCE_INDEX_BITS,
    ELF_HEAP_GUARD_SIZE            = 32,
    ELF_HEAP_GUARD_VALUE           = 0xa5,
    ELF_GRAPHICS_COMMAND_CAPACITY  = 64,
};

_Static_assert((1U << ELF_SOCKET_INDEX_BITS) == ELF_SOCKET_CAPACITY, "ELF socket index bits must match capacity");
_Static_assert((1U << ELF_TLS_INDEX_BITS) == ELF_TLS_CAPACITY, "ELF TLS index bits must match capacity");
_Static_assert((1U << ELF_WAIT_SOURCE_INDEX_BITS) == ELF_WAIT_SOURCE_CAPACITY,
               "ELF wait-source index bits must match capacity");

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

typedef struct {
        int platform_socket;
        tabos_wait_source_t wait_source;
        uint32_t generation;
        bool open;
} elf_socket_t;

typedef enum {
    ELF_WAIT_SOURCE_SOCKET,
    ELF_WAIT_SOURCE_DEVICE_SUBSCRIPTION,
    ELF_WAIT_SOURCE_TYPE_COUNT,
} elf_wait_source_type_t;

typedef int (*elf_wait_source_poll_fn)(loader_elf_application_t* application, uintptr_t parent,
                                       uint32_t requested_events, uint32_t* returned_events);
typedef int (*elf_wait_source_socket_fn)(loader_elf_application_t* application, uintptr_t parent,
                                         platform_network_wait_item_t* item);

typedef struct {
        uint32_t valid_events;
        elf_wait_source_poll_fn poll;
        elf_wait_source_socket_fn socket;
} elf_wait_source_adapter_t;

typedef struct {
        uintptr_t parent;
        uint32_t generation;
        elf_wait_source_type_t type;
        bool open;
} elf_wait_source_t;

typedef struct {
        int platform_connection;
        uint32_t generation;
        bool open;
} elf_tls_t;

struct loader_elf_application {
        tabos_app_descriptor_t descriptor;
        char name[TABOS_FS_NAME_MAX + 1U];
        char path[TABOS_FS_PATH_MAX];
        size_t argc;
        char argument_data[TABOS_ELF_ARG_BYTES_MAX];
        const char* argv[TABOS_ELF_ARG_MAX + 1U];
        tabos_app_context_t* context;
        const tabos_console_session_t* console;
        loader_elf_image_t image;
        platform_riscv32_context_t* execution;
        atomic_bool exit_requested;
        atomic_int requested_exit_status;
        atomic_bool exec_requested;
        atomic_bool exec_in_flight;
        atomic_bool exec_status_ready;
        bool graphics_active;
        uint32_t graphics_overlay_flags;
        elf_graphics_command_t graphics_commands[ELF_GRAPHICS_COMMAND_CAPACITY];
        size_t graphics_command_head;
        size_t graphics_command_count;
        atomic_int exec_status;
        char exec_path[TABOS_FS_PATH_MAX];
        size_t exec_argc;
        char exec_argument_data[TABOS_ELF_ARG_BYTES_MAX];
        const char* exec_argv[TABOS_ELF_ARG_MAX + 1U];
        elf_descriptor_t descriptors[ELF_DESCRIPTOR_CAPACITY];
        elf_socket_t sockets[ELF_SOCKET_CAPACITY];
        elf_wait_source_t wait_sources[ELF_WAIT_SOURCE_CAPACITY];
        atomic_bool wait_active;
        atomic_bool wait_cancel_requested;
        elf_tls_t tls[ELF_TLS_CAPACITY];
        char working_directory[TABOS_FS_PATH_MAX];
        uint8_t* heap_allocation;
        uint8_t* heap;
        size_t heap_used;
        size_t heap_limit;
        uint32_t tty_mode;
        char input_pending[4];
        uint8_t input_pending_offset;
        uint8_t input_pending_length;
};

static atomic_uint next_wait_source_generation = 1U;

static bool elf_handle_tty_navigation(loader_elf_application_t* application, const tabos_input_event_t* event)
{
    if (application == NULL || event == NULL || application->graphics_active ||
        (application->tty_mode & TABOS_TTY_MODE_SCROLL_KEYS) == 0U || event->type != TABOS_INPUT_KEY_DOWN) {
        return false;
    }
    if (event->key == TABOS_KEY_PAGE_UP ||
        (event->key == TABOS_KEY_UP && (event->modifiers & TABOS_MODIFIER_CONTROL) != 0U)) {
        (void) tabos_console_page_up(application->console);
        return true;
    }
    if (event->key == TABOS_KEY_PAGE_DOWN ||
        (event->key == TABOS_KEY_DOWN && (event->modifiers & TABOS_MODIFIER_CONTROL) != 0U)) {
        (void) tabos_console_page_down(application->console);
        return true;
    }
    if (event->key == TABOS_KEY_HOME ||
        (event->key == TABOS_KEY_LEFT && (event->modifiers & TABOS_MODIFIER_CONTROL) != 0U)) {
        (void) tabos_console_scroll_to_start(application->console);
        return true;
    }
    if (event->key == TABOS_KEY_END ||
        (event->key == TABOS_KEY_RIGHT && (event->modifiers & TABOS_MODIFIER_CONTROL) != 0U)) {
        (void) tabos_console_scroll_to_end(application->console);
        return true;
    }
    return false;
}

static bool copy_arguments(size_t argc, const char* const* argv, char* data, size_t data_size, const char** copied_argv)
{
    if (argc > TABOS_ELF_ARG_MAX || (argc > 0U && argv == NULL)) {
        return false;
    }
    argv = platform_executable_data_pointer(argv, (argc + 1U) * sizeof(*argv));
    if (argv == NULL) {
        return false;
    }
    size_t used = 0U;
    for (size_t index = 0U; index < argc; ++index) {
        if (argv[index] == NULL) {
            return false;
        }
        const char* argument = platform_executable_data_pointer(argv[index], data_size - used);
        if (argument == NULL) {
            return false;
        }
        const size_t length = strlen(argument) + 1U;
        if (length > data_size - used) {
            return false;
        }
        copied_argv[index] = data + used;
        memcpy(data + used, argument, length);
        used += length;
    }
    copied_argv[argc] = NULL;
    return true;
}

static loader_elf_application_t* application_from_context(tabos_app_context_t* context)
{
    return context != NULL ? context->application_data : NULL;
}

static void elf_console_write(const char* text)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || application->console == NULL || text == NULL) {
        return;
    }
    const char* readable_text = platform_executable_data_pointer(text, TABOS_ELF_ARG_BYTES_MAX);
    if (readable_text == NULL) {
        return;
    }
    (void) tabos_console_write_line(application->console, readable_text);
}

static void elf_console_write_raw(const char* text)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || application->console == NULL || text == NULL) {
        return;
    }
    const char* readable_text = platform_executable_data_pointer(text, TABOS_ELF_ARG_BYTES_MAX);
    if (readable_text == NULL) {
        return;
    }
    (void) tabos_console_write(application->console, readable_text);
}

static void elf_request_exit(int exit_status)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || application->context == NULL) {
        return;
    }
    atomic_store_explicit(&application->requested_exit_status, exit_status, memory_order_release);
    atomic_store_explicit(&application->exit_requested, true, memory_order_release);
}

static int elf_console_read(char* buffer, uint32_t capacity)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || application->console == NULL || buffer == NULL || capacity < 2U) {
        return -1;
    }
    tabos_input_event_t event;
    while (tabos_console_poll(application->console, &event)) {
        if (event.type == TABOS_INPUT_TEXT) {
            const size_t length = strlen(event.text);
            const size_t copied = length < (size_t) capacity - 1U ? length : (size_t) capacity - 1U;
            memcpy(buffer, event.text, copied);
            buffer[copied] = '\0';
            return (int) copied;
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
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    return application != NULL && application->console != NULL && tabos_console_clear(application->console) ? 0 : -1;
}

static int elf_fs_getcwd(char* buffer, uint32_t capacity)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || buffer == NULL || capacity == 0U) {
        return -TABOS_EINVAL;
    }
    const size_t needed = strlen(application->working_directory) + 1U;
    if (needed > capacity) {
        return -TABOS_ENAMETOOLONG;
    }
    memcpy(buffer, application->working_directory, needed);
    return 0;
}

static int elf_fs_chdir(const char* path)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || path == NULL) {
        return -TABOS_EINVAL;
    }
    char resolved[TABOS_FS_PATH_MAX];
    const char* readable = platform_executable_data_pointer(path, TABOS_FS_PATH_MAX);
    if (readable == NULL) {
        return -TABOS_EIO;
    }
    if (!filesystem_normalize_path(readable, application->working_directory, resolved, sizeof(resolved))) {
        return -TABOS_EINVAL;
    }
    tabos_stat_t status;
    if (tabos_fs_stat(resolved, &status) != 0) {
        return -*tabos_errno_location();
    }
    if ((status.mode & TABOS_S_IFDIR) == 0U) {
        return -TABOS_ENOTDIR;
    }
    memcpy(application->working_directory, resolved, strlen(resolved) + 1U);
    return 0;
}

static bool elf_resolve_path(loader_elf_application_t* application, const char* path, char resolved[TABOS_FS_PATH_MAX])
{
    if (application == NULL || path == NULL) {
        return false;
    }
    const char* readable = platform_executable_data_pointer(path, TABOS_FS_PATH_MAX);
    return readable != NULL &&
           filesystem_normalize_path(readable, application->working_directory, resolved, TABOS_FS_PATH_MAX);
}

static int elf_fd_open(const char* path, int flags, uint32_t mode)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    char resolved[TABOS_FS_PATH_MAX];
    if (!elf_resolve_path(application, path, resolved)) {
        return -TABOS_EINVAL;
    }
    size_t index = 3U;
    while (index < ELF_DESCRIPTOR_CAPACITY && application->descriptors[index].open) {
        ++index;
    }
    if (index == ELF_DESCRIPTOR_CAPACITY) {
        return -TABOS_EMFILE;
    }
    const tabos_fd_t kernel_descriptor = tabos_fs_open(resolved, flags & ~TABOS_O_NONBLOCK, mode);
    if (kernel_descriptor < 0) {
        return -*tabos_errno_location();
    }
    application->descriptors[index] = (elf_descriptor_t) {
        .kernel_descriptor = kernel_descriptor,
        .flags             = flags,
        .open              = true,
    };
    return (int) index;
}

static int elf_fd_close(int descriptor)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || descriptor < 3 || descriptor >= ELF_DESCRIPTOR_CAPACITY ||
        !application->descriptors[descriptor].open) {
        return -TABOS_EBADF;
    }
    const int result = tabos_fs_close(application->descriptors[descriptor].kernel_descriptor);
    if (result != 0) {
        return -*tabos_errno_location();
    }
    application->descriptors[descriptor] = (elf_descriptor_t) {0};
    return 0;
}

static int elf_copy_pending_input(loader_elf_application_t* application, void* buffer, uint32_t count)
{
    const uint32_t available = (uint32_t) application->input_pending_length - application->input_pending_offset;
    const uint32_t copied    = available < count ? available : count;
    if (copied == 0U) {
        return 0;
    }
    memcpy(buffer, application->input_pending + application->input_pending_offset, copied);
    application->input_pending_offset = (uint8_t) (application->input_pending_offset + copied);
    if (application->input_pending_offset == application->input_pending_length) {
        application->input_pending_offset = 0U;
        application->input_pending_length = 0U;
    }
    return (int) copied;
}

static bool elf_queue_arrow_input(loader_elf_application_t* application, const tabos_input_event_t* event)
{
    if (event->type != TABOS_INPUT_KEY_DOWN) {
        return false;
    }
    char final = '\0';
    switch (event->key) {
        case TABOS_KEY_UP: final = 'A'; break;
        case TABOS_KEY_DOWN: final = 'B'; break;
        case TABOS_KEY_RIGHT: final = 'C'; break;
        case TABOS_KEY_LEFT: final = 'D'; break;
        default: return false;
    }
    application->input_pending[0]     = '\x1b';
    application->input_pending[1]     = '[';
    application->input_pending[2]     = final;
    application->input_pending_offset = 0U;
    application->input_pending_length = 3U;
    return true;
}

static int elf_input_poll(tabos_input_event_t* event)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || application->console == NULL || event == NULL) {
        return -TABOS_EINVAL;
    }
    while (tabos_console_poll(application->console, event)) {
        if (elf_handle_tty_navigation(application, event)) {
            continue;
        }
        if ((application->tty_mode & TABOS_TTY_MODE_RAW_INPUT) != 0U && event->type == TABOS_INPUT_TEXT) {
            continue;
        }
        return 1;
    }
    return 0;
}

static int elf_fd_read(int descriptor, void* buffer, uint32_t count)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || (buffer == NULL && count != 0U)) {
        return -TABOS_EINVAL;
    }
    if (descriptor == 0) {
        if (count == 0U) {
            return 0;
        }
        const int pending = elf_copy_pending_input(application, buffer, count);
        if (pending != 0) {
            return pending;
        }
        tabos_input_event_t event;
        while (tabos_console_poll(application->console, &event)) {
            if (elf_handle_tty_navigation(application, &event)) {
                continue;
            }
            if (event.type == TABOS_INPUT_TEXT) {
                const size_t length = strlen(event.text);
                const size_t copied = length < count ? length : count;
                memcpy(buffer, event.text, copied);
                return (int) copied;
            }
            if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_BACKSPACE) {
                ((char*) buffer)[0] = '\b';
                return 1;
            }
            if (elf_queue_arrow_input(application, &event)) {
                return elf_copy_pending_input(application, buffer, count);
            }
        }
        return -TABOS_EAGAIN;
    }
    if (descriptor < 3 || descriptor >= ELF_DESCRIPTOR_CAPACITY || !application->descriptors[descriptor].open) {
        return -TABOS_EBADF;
    }
    const tabos_ssize_t result = tabos_fs_read(application->descriptors[descriptor].kernel_descriptor, buffer, count);
    return result >= 0 ? (int) result : -*tabos_errno_location();
}

static int elf_tty_get_mode(int descriptor)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || descriptor < 0 || descriptor > 2) {
        return -TABOS_ENOTTY;
    }
    return (int) application->tty_mode;
}

static int elf_tty_set_mode(int descriptor, uint32_t mode)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || descriptor < 0 || descriptor > 2) {
        return -TABOS_ENOTTY;
    }
    if ((mode & ~(uint32_t) (TABOS_TTY_MODE_SCROLL_KEYS | TABOS_TTY_MODE_RAW_INPUT)) != 0U) {
        return -TABOS_EINVAL;
    }
    application->tty_mode = mode;
    return 0;
}

static int elf_fd_write(int descriptor, const void* buffer, uint32_t count)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || (buffer == NULL && count != 0U)) {
        return -TABOS_EINVAL;
    }
    if (descriptor == 1 || descriptor == 2) {
        const char* source = platform_executable_data_pointer(buffer, count);
        if (source == NULL) {
            return -TABOS_EIO;
        }
        char chunk[129];
        uint32_t written = 0U;
        while (written < count) {
            const uint32_t length =
                count - written < sizeof(chunk) - 1U ? count - written : (uint32_t) sizeof(chunk) - 1U;
            memcpy(chunk, source + written, length);
            chunk[length] = '\0';
            if (!tabos_console_write(application->console, chunk)) {
                return -TABOS_EIO;
            }
            written += length;
        }
        return (int) count;
    }
    if (descriptor < 3 || descriptor >= ELF_DESCRIPTOR_CAPACITY || !application->descriptors[descriptor].open) {
        return -TABOS_EBADF;
    }
    const void* source = platform_executable_data_pointer(buffer, count);
    if (source == NULL) {
        return -TABOS_EIO;
    }
    const tabos_ssize_t result = tabos_fs_write(application->descriptors[descriptor].kernel_descriptor, source, count);
    return result >= 0 ? (int) result : -*tabos_errno_location();
}

static int elf_fd_seek(int descriptor, int32_t offset, int whence, int32_t* position)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || position == NULL || descriptor < 3 || descriptor >= ELF_DESCRIPTOR_CAPACITY ||
        !application->descriptors[descriptor].open) {
        return -TABOS_EBADF;
    }
    const tabos_off_t result = tabos_fs_seek(application->descriptors[descriptor].kernel_descriptor, offset, whence);
    if (result < 0) {
        return -*tabos_errno_location();
    }
    if (result > INT32_MAX) {
        return -TABOS_ENOTSUP;
    }
    *position = (int32_t) result;
    return 0;
}

static void elf_copy_stat(tabos_elf_stat_t* destination, const tabos_stat_t* source)
{
    destination->mode               = source->mode;
    destination->size_low           = (uint32_t) source->size;
    destination->size_high          = (uint32_t) (source->size >> 32U);
    destination->modified_time_low  = (int32_t) source->modified_time;
    destination->modified_time_high = (int32_t) (source->modified_time >> 32U);
}

static int elf_fs_stat_path(const char* path, tabos_elf_stat_t* status)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    char resolved[TABOS_FS_PATH_MAX];
    tabos_stat_t native_status;
    if (status == NULL || !elf_resolve_path(application, path, resolved)) {
        return -TABOS_EINVAL;
    }
    if (tabos_fs_stat(resolved, &native_status) != 0) {
        return -*tabos_errno_location();
    }
    elf_copy_stat(status, &native_status);
    return 0;
}

static int elf_fd_stat(int descriptor, tabos_elf_stat_t* status)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (status == NULL || application == NULL) {
        return -TABOS_EINVAL;
    }
    if (descriptor >= 0 && descriptor <= 2) {
        *status = (tabos_elf_stat_t) {0};
        return 0;
    }
    if (descriptor < 3 || descriptor >= ELF_DESCRIPTOR_CAPACITY || !application->descriptors[descriptor].open) {
        return -TABOS_EBADF;
    }
    tabos_stat_t native_status;
    if (tabos_fs_fstat(application->descriptors[descriptor].kernel_descriptor, &native_status) != 0) {
        return -*tabos_errno_location();
    }
    elf_copy_stat(status, &native_status);
    return 0;
}

static int elf_fs_mkdir_path(const char* path, uint32_t mode)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    char resolved[TABOS_FS_PATH_MAX];
    if (!elf_resolve_path(application, path, resolved)) {
        return -TABOS_EINVAL;
    }
    return tabos_fs_mkdir(resolved, mode) == 0 ? 0 : -*tabos_errno_location();
}

static int elf_fs_unlink_path(const char* path)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    char resolved[TABOS_FS_PATH_MAX];
    if (!elf_resolve_path(application, path, resolved)) {
        return -TABOS_EINVAL;
    }
    return tabos_fs_unlink(resolved) == 0 ? 0 : -*tabos_errno_location();
}

static int elf_fs_rmdir_path(const char* path)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    char resolved[TABOS_FS_PATH_MAX];
    if (!elf_resolve_path(application, path, resolved)) {
        return -TABOS_EINVAL;
    }
    return tabos_fs_rmdir(resolved) == 0 ? 0 : -*tabos_errno_location();
}

static int elf_fs_rename_path(const char* old_path, const char* new_path)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    char old_resolved[TABOS_FS_PATH_MAX];
    char new_resolved[TABOS_FS_PATH_MAX];
    if (!elf_resolve_path(application, old_path, old_resolved) ||
        !elf_resolve_path(application, new_path, new_resolved)) {
        return -TABOS_EINVAL;
    }
    return tabos_fs_rename(old_resolved, new_resolved) == 0 ? 0 : -*tabos_errno_location();
}

static int elf_fd_get_flags(int descriptor)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || descriptor < 0 || descriptor >= ELF_DESCRIPTOR_CAPACITY ||
        !application->descriptors[descriptor].open) {
        return -TABOS_EBADF;
    }
    return application->descriptors[descriptor].flags;
}

static int elf_fd_set_flags(int descriptor, int flags)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || descriptor < 0 || descriptor >= ELF_DESCRIPTOR_CAPACITY ||
        !application->descriptors[descriptor].open) {
        return -TABOS_EBADF;
    }
    application->descriptors[descriptor].flags =
        (application->descriptors[descriptor].flags & ~TABOS_O_NONBLOCK) | (flags & TABOS_O_NONBLOCK);
    return 0;
}

static void* elf_heap_sbrk(int32_t increment)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || increment < 0 || application->heap_limit < application->heap_used ||
        (size_t) increment > application->heap_limit - application->heap_used) {
        return (void*) -1;
    }
    if (application->heap == NULL) {
        application->heap_allocation = malloc(application->heap_limit + (2U * ELF_HEAP_GUARD_SIZE));
        if (application->heap_allocation == NULL) {
            return (void*) -1;
        }
        memset(application->heap_allocation, ELF_HEAP_GUARD_VALUE, ELF_HEAP_GUARD_SIZE);
        memset(application->heap_allocation + ELF_HEAP_GUARD_SIZE + application->heap_limit, ELF_HEAP_GUARD_VALUE,
               ELF_HEAP_GUARD_SIZE);
        application->heap = application->heap_allocation + ELF_HEAP_GUARD_SIZE;
    }
    void* previous          = application->heap + application->heap_used;
    application->heap_used += (size_t) increment;
    return previous;
}

static int elf_fs_list(const char* path, char* buffer, uint32_t capacity)
{
    if (path == NULL || buffer == NULL || capacity == 0U) {
        return -TABOS_EINVAL;
    }
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL) {
        return -TABOS_EINVAL;
    }
    char resolved[TABOS_FS_PATH_MAX];
    if (!filesystem_normalize_path(path, application->working_directory, resolved, sizeof(resolved))) {
        return -TABOS_ENAMETOOLONG;
    }
    const tabos_dir_t directory = tabos_fs_opendir(resolved);
    if (directory < 0) {
        return -*tabos_errno_location();
    }
    size_t used = 0U;
    tabos_dirent_t entry;
    int result = 0;
    while ((result = tabos_fs_readdir(directory, &entry)) > 0) {
        const size_t length = strlen(entry.name);
        if (length + 3U >= (size_t) capacity - used) {
            result = -TABOS_ENOSPC;
            break;
        }
        buffer[used++] = (entry.mode & TABOS_S_IFDIR) != 0U ? 'D' : 'F';
        buffer[used++] = ':';
        memcpy(buffer + used, entry.name, length);
        used           += length;
        buffer[used++]  = '\n';
    }
    if (result < 0) {
        result = -*tabos_errno_location();
    }
    if (tabos_fs_closedir(directory) != 0 && result == 0) {
        result = -*tabos_errno_location();
    }
    buffer[used] = '\0';
    return result;
}

static uint64_t elf_monotonic_ms(void)
{
    return platform_time_ms();
}

static int elf_wall_time_get(tabos_elf_wall_time_t* time)
{
    tabos_elf_wall_time_t* writable_time =
        (tabos_elf_wall_time_t*) platform_executable_data_pointer(time, sizeof(*time));
    int64_t seconds = 0;
    if (writable_time == NULL) {
        return -TABOS_EINVAL;
    }
    if (!platform_wall_clock_get(&seconds)) {
        return -TABOS_EIO;
    }
    writable_time->seconds_low  = (uint32_t) seconds;
    writable_time->seconds_high = (int32_t) (seconds >> 32U);
    return 0;
}

static int elf_wall_time_set(const tabos_elf_wall_time_t* time)
{
    const tabos_elf_wall_time_t* readable_time = platform_executable_data_pointer(time, sizeof(*time));
    if (readable_time == NULL || readable_time->seconds_high < 0) {
        return -TABOS_EINVAL;
    }
    const int64_t seconds =
        (int64_t) ((uint64_t) readable_time->seconds_low | (uint64_t) (uint32_t) readable_time->seconds_high << 32U);
    return platform_wall_clock_set(seconds) ? 0 : -TABOS_EIO;
}

static int elf_system_action(uint32_t action)
{
    platform_system_action_t platform_action;
    if (action == TABOS_ELF_SYSTEM_REBOOT) {
        platform_action = PLATFORM_SYSTEM_ACTION_REBOOT;
    } else if (action == TABOS_ELF_SYSTEM_POWER_OFF) {
        platform_action = PLATFORM_SYSTEM_ACTION_POWER_OFF;
    } else {
        return -TABOS_EINVAL;
    }
    return kernel_runtime_request_system_action(platform_action) ? 0 : -TABOS_EBUSY;
}

static int elf_network_status(tabos_elf_network_status_t* status)
{
    tabos_elf_network_status_t* writable_status =
        (tabos_elf_network_status_t*) platform_executable_data_pointer(status, sizeof(*status));
    network_status_t source;
    if (writable_status == NULL) {
        return -TABOS_EINVAL;
    }
    if (!network_service_status(&source)) {
        return -TABOS_EIO;
    }
    memset(writable_status, 0, sizeof(*writable_status));
    writable_status->state        = (uint32_t) source.state;
    writable_status->signal_dbm   = source.signal_dbm;
    writable_status->attempts     = source.attempts;
    writable_status->auto_connect = source.auto_connect ? 1U : 0U;
    writable_status->saved_config = source.config_available ? 1U : 0U;
    (void) snprintf(writable_status->hostname, sizeof(writable_status->hostname), "%s", source.hostname);
    (void) snprintf(writable_status->ssid, sizeof(writable_status->ssid), "%s", source.ssid);
    (void) snprintf(writable_status->ipv4, sizeof(writable_status->ipv4), "%s", source.ipv4);
    (void) snprintf(writable_status->last_failure, sizeof(writable_status->last_failure), "%s", source.last_failure);
    return 0;
}

static int elf_network_connect_saved(void)
{
    network_config_t config;
    const network_config_result_t result = network_config_load(&config);
    if (result == NETWORK_CONFIG_NOT_FOUND) {
        return -TABOS_ENOENT;
    }
    if (result != NETWORK_CONFIG_OK) {
        return -TABOS_EIO;
    }
    if (!network_service_connect(config.ssid, config.password, config.auto_connect)) {
        return -TABOS_EIO;
    }
    hardware_devices_update();
    return 0;
}

static int elf_network_disconnect(void)
{
    if (!network_service_disconnect()) {
        return -TABOS_EIO;
    }
    hardware_devices_update();
    return 0;
}

static int network_operation_error(network_operation_result_t result)
{
    switch (result) {
        case NETWORK_OPERATION_OK: return 0;
        case NETWORK_OPERATION_INVALID: return -TABOS_EINVAL;
        case NETWORK_OPERATION_OFFLINE: return -TABOS_ENETDOWN;
        case NETWORK_OPERATION_NOT_FOUND: return -TABOS_ENOENT;
        case NETWORK_OPERATION_TIMEOUT: return -TABOS_ETIMEDOUT;
        case NETWORK_OPERATION_UNSUPPORTED: return -TABOS_ENOTSUP;
        case NETWORK_OPERATION_IO: return -TABOS_EIO;
    }
    return -TABOS_EIO;
}

static int elf_network_resolve(const char* hostname, uint32_t family, tabos_elf_network_address_t* address)
{
    const char* readable_hostname = platform_executable_data_pointer(hostname, 254U);
    tabos_elf_network_address_t* writable_address =
        (tabos_elf_network_address_t*) platform_executable_data_pointer(address, sizeof(*address));
    if (readable_hostname == NULL || writable_address == NULL) {
        return -TABOS_EINVAL;
    }
    const size_t length = strnlen(readable_hostname, 254U);
    if (length == 0U || length == 254U) {
        return -TABOS_EINVAL;
    }
    network_address_t resolved;
    const network_operation_result_t operation = network_service_resolve(readable_hostname, family, &resolved);
    if (operation != NETWORK_OPERATION_OK) {
        return network_operation_error(operation);
    }
    memset(writable_address, 0, sizeof(*writable_address));
    writable_address->family = resolved.family;
    (void) snprintf(writable_address->text, sizeof(writable_address->text), "%s", resolved.text);
    return 0;
}

static int elf_network_echo(const tabos_elf_network_address_t* address, uint32_t sequence, uint32_t payload_bytes,
                            uint32_t timeout_ms, tabos_elf_network_echo_result_t* result)
{
    const tabos_elf_network_address_t* readable_address = platform_executable_data_pointer(address, sizeof(*address));
    tabos_elf_network_echo_result_t* writable_result =
        (tabos_elf_network_echo_result_t*) platform_executable_data_pointer(result, sizeof(*result));
    if (readable_address == NULL || writable_result == NULL || sequence > UINT16_MAX || payload_bytes > UINT16_MAX) {
        return -TABOS_EINVAL;
    }
    network_address_t target = {.family = readable_address->family};
    memcpy(target.text, readable_address->text, sizeof(target.text));
    target.text[sizeof(target.text) - 1U] = '\0';
    network_echo_result_t echoed;
    const network_operation_result_t operation =
        network_service_echo(&target, (uint16_t) sequence, (uint16_t) payload_bytes, timeout_ms, &echoed);
    if (operation != NETWORK_OPERATION_OK) {
        return network_operation_error(operation);
    }
    writable_result->sequence      = echoed.sequence;
    writable_result->bytes         = echoed.bytes;
    writable_result->round_trip_ms = echoed.round_trip_ms;
    return 0;
}

static elf_socket_t* elf_socket(loader_elf_application_t* application, int socket)
{
    if (application == NULL || socket <= 0) {
        return NULL;
    }
    const uint32_t handle     = (uint32_t) socket;
    const uint32_t index      = handle & ELF_SOCKET_INDEX_MASK;
    const uint32_t generation = handle >> ELF_SOCKET_INDEX_BITS;
    elf_socket_t* owned       = &application->sockets[index];
    if (!owned->open || generation == 0U || owned->generation != generation) {
        return NULL;
    }
    return owned;
}

static elf_wait_source_t* elf_wait_source(loader_elf_application_t* application, tabos_wait_source_t source)
{
    if (application == NULL || source < 0) {
        return NULL;
    }
    const uint32_t handle     = (uint32_t) source;
    const uint32_t index      = handle & ELF_WAIT_SOURCE_INDEX_MASK;
    const uint32_t generation = handle >> ELF_WAIT_SOURCE_INDEX_BITS;
    elf_wait_source_t* owned  = &application->wait_sources[index];
    if (!owned->open || generation == 0U || owned->generation != generation) {
        return NULL;
    }
    return owned;
}

static tabos_wait_source_t elf_wait_source_allocate(loader_elf_application_t* application, elf_wait_source_type_t type,
                                                    uintptr_t parent)
{
    for (uint32_t index = 0U; index < ELF_WAIT_SOURCE_CAPACITY; ++index) {
        if (application->wait_sources[index].open) {
            continue;
        }
        uint32_t generation = atomic_fetch_add_explicit(&next_wait_source_generation, 1U, memory_order_relaxed);
        if (generation == 0U || generation > ELF_WAIT_SOURCE_GENERATION_MAX) {
            generation = 1U;
            atomic_store_explicit(&next_wait_source_generation, 2U, memory_order_relaxed);
        }
        application->wait_sources[index] = (elf_wait_source_t) {
            .parent     = parent,
            .generation = generation,
            .type       = type,
            .open       = true,
        };
        return (tabos_wait_source_t) ((generation << ELF_WAIT_SOURCE_INDEX_BITS) | index);
    }
    return TABOS_WAIT_SOURCE_INVALID;
}

static void elf_wait_source_invalidate(loader_elf_application_t* application, tabos_wait_source_t source)
{
    elf_wait_source_t* owned = elf_wait_source(application, source);
    if (owned == NULL) {
        return;
    }
    const uint32_t generation = owned->generation;
    *owned                    = (elf_wait_source_t) {.generation = generation};
}

static tabos_wait_source_t elf_wait_source_find(const loader_elf_application_t* application,
                                                elf_wait_source_type_t type, uintptr_t parent)
{
    if (application == NULL) {
        return TABOS_WAIT_SOURCE_INVALID;
    }
    for (uint32_t index = 0U; index < ELF_WAIT_SOURCE_CAPACITY; ++index) {
        const elf_wait_source_t* source = &application->wait_sources[index];
        if (source->open && source->type == type && source->parent == parent) {
            return (tabos_wait_source_t) ((source->generation << ELF_WAIT_SOURCE_INDEX_BITS) | index);
        }
    }
    return TABOS_WAIT_SOURCE_INVALID;
}

static int elf_socket_allocate(loader_elf_application_t* application, int platform_socket)
{
    for (int index = 0; index < ELF_SOCKET_CAPACITY; ++index) {
        if (!application->sockets[index].open) {
            uint32_t generation = application->sockets[index].generation + 1U;
            if (generation == 0U || generation > ELF_SOCKET_GENERATION_MAX) {
                generation = 1U;
            }
            const int socket = (int) ((generation << ELF_SOCKET_INDEX_BITS) | (uint32_t) index);
            const tabos_wait_source_t source =
                elf_wait_source_allocate(application, ELF_WAIT_SOURCE_SOCKET, (uintptr_t) socket);
            if (source == TABOS_WAIT_SOURCE_INVALID) {
                (void) platform_network_socket_close(platform_socket);
                return -TABOS_ENFILE;
            }
            application->sockets[index] = (elf_socket_t) {
                .platform_socket = platform_socket,
                .wait_source     = source,
                .generation      = generation,
                .open            = true,
            };
            return socket;
        }
    }
    (void) platform_network_socket_close(platform_socket);
    return -TABOS_EMFILE;
}

static int elf_socket_open(uint32_t family, uint32_t type)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL) {
        return -TABOS_EIO;
    }
    const int platform_socket = platform_network_socket_open(family, type);
    return platform_socket < 0 ? platform_socket : elf_socket_allocate(application, platform_socket);
}

static int elf_socket_close(int socket)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    elf_socket_t* owned                   = elf_socket(application, socket);
    if (owned == NULL) {
        return -TABOS_EBADF;
    }
    elf_wait_source_invalidate(application, owned->wait_source);
    const uint32_t generation = owned->generation;
    const int result          = platform_network_socket_close(owned->platform_socket);
    *owned                    = (elf_socket_t) {.generation = generation};
    return result;
}

static int elf_socket_wait_source(int socket)
{
    elf_socket_t* owned = elf_socket(platform_riscv32_current_user_data(), socket);
    return owned != NULL ? owned->wait_source : -TABOS_EBADF;
}

static bool elf_socket_endpoint(const tabos_elf_socket_endpoint_t* endpoint, platform_network_address_t* address,
                                uint16_t* port)
{
    const tabos_elf_socket_endpoint_t* readable = platform_executable_data_pointer(endpoint, sizeof(*endpoint));
    if (readable == NULL || address == NULL || port == NULL || readable->port > UINT16_MAX) {
        return false;
    }
    address->family = readable->address.family;
    memcpy(address->text, readable->address.text, sizeof(address->text));
    address->text[sizeof(address->text) - 1U] = '\0';
    *port                                     = (uint16_t) readable->port;
    return true;
}

static void elf_socket_write_endpoint(tabos_elf_socket_endpoint_t* endpoint, const platform_network_address_t* address,
                                      uint16_t port)
{
    memset(endpoint, 0, sizeof(*endpoint));
    endpoint->address.family = address->family;
    memcpy(endpoint->address.text, address->text, sizeof(endpoint->address.text));
    endpoint->port = port;
}

static int elf_socket_bind(int socket, const tabos_elf_socket_endpoint_t* endpoint)
{
    elf_socket_t* owned = elf_socket(platform_riscv32_current_user_data(), socket);
    platform_network_address_t address;
    uint16_t port;
    if (owned == NULL) {
        return -TABOS_EBADF;
    }
    if (!elf_socket_endpoint(endpoint, &address, &port)) {
        return -TABOS_EINVAL;
    }
    return platform_network_socket_bind(owned->platform_socket, &address, port);
}

static int elf_socket_get_local_endpoint(int socket, tabos_elf_socket_endpoint_t* endpoint)
{
    elf_socket_t* owned = elf_socket(platform_riscv32_current_user_data(), socket);
    tabos_elf_socket_endpoint_t* writable =
        (tabos_elf_socket_endpoint_t*) platform_executable_data_pointer(endpoint, sizeof(*endpoint));
    if (owned == NULL) {
        return -TABOS_EBADF;
    }
    if (writable == NULL) {
        return -TABOS_EINVAL;
    }
    platform_network_address_t address;
    uint16_t port;
    const int result = platform_network_socket_get_local_endpoint(owned->platform_socket, &address, &port);
    if (result == 0) {
        elf_socket_write_endpoint(writable, &address, port);
    }
    return result;
}

static int elf_socket_listen(int socket, uint32_t backlog)
{
    elf_socket_t* owned = elf_socket(platform_riscv32_current_user_data(), socket);
    if (owned == NULL) {
        return -TABOS_EBADF;
    }
    return backlog == 0U || backlog > UINT16_MAX ?
               -TABOS_EINVAL :
               platform_network_socket_listen(owned->platform_socket, (uint16_t) backlog);
}

static int elf_socket_accept(int socket, tabos_elf_socket_endpoint_t* peer)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    elf_socket_t* owned                   = elf_socket(application, socket);
    tabos_elf_socket_endpoint_t* writable =
        peer != NULL ? (tabos_elf_socket_endpoint_t*) platform_executable_data_pointer(peer, sizeof(*peer)) : NULL;
    if (owned == NULL) {
        return -TABOS_EBADF;
    }
    if (peer != NULL && writable == NULL) {
        return -TABOS_EINVAL;
    }
    platform_network_address_t address;
    uint16_t port;
    const int accepted = platform_network_socket_accept(owned->platform_socket, &address, &port);
    if (accepted < 0) {
        return accepted;
    }
    const int result = elf_socket_allocate(application, accepted);
    if (result >= 0 && writable != NULL) {
        elf_socket_write_endpoint(writable, &address, port);
    }
    return result;
}

static int elf_socket_connect(int socket, const tabos_elf_socket_endpoint_t* endpoint)
{
    elf_socket_t* owned = elf_socket(platform_riscv32_current_user_data(), socket);
    platform_network_address_t address;
    uint16_t port;
    if (owned == NULL) {
        return -TABOS_EBADF;
    }
    if (!elf_socket_endpoint(endpoint, &address, &port)) {
        return -TABOS_EINVAL;
    }
    return platform_network_socket_connect(owned->platform_socket, &address, port);
}

static int elf_socket_set_nonblocking(int socket, uint32_t enabled)
{
    elf_socket_t* owned = elf_socket(platform_riscv32_current_user_data(), socket);
    if (owned == NULL) {
        return -TABOS_EBADF;
    }
    if (enabled > 1U) {
        return -TABOS_EINVAL;
    }
    return platform_network_socket_set_nonblocking(owned->platform_socket, enabled != 0U);
}

static int elf_socket_shutdown(int socket, uint32_t direction)
{
    elf_socket_t* owned = elf_socket(platform_riscv32_current_user_data(), socket);
    if (owned == NULL) {
        return -TABOS_EBADF;
    }
    if (direction > 2U) {
        return -TABOS_EINVAL;
    }
    return platform_network_socket_shutdown(owned->platform_socket, direction);
}

static int elf_socket_send(int socket, const void* data, uint32_t size)
{
    elf_socket_t* owned  = elf_socket(platform_riscv32_current_user_data(), socket);
    const void* readable = platform_executable_data_pointer(data, size);
    if (owned == NULL) {
        return -TABOS_EBADF;
    }
    if (readable == NULL || size == 0U || size > TABOS_NETWORK_IO_MAX) {
        return -TABOS_EINVAL;
    }
    return platform_network_socket_send(owned->platform_socket, readable, size);
}

static int elf_socket_receive(int socket, void* data, uint32_t capacity)
{
    elf_socket_t* owned = elf_socket(platform_riscv32_current_user_data(), socket);
    void* writable      = (void*) platform_executable_data_pointer(data, capacity);
    if (owned == NULL) {
        return -TABOS_EBADF;
    }
    if (writable == NULL || capacity == 0U || capacity > TABOS_NETWORK_IO_MAX) {
        return -TABOS_EINVAL;
    }
    return platform_network_socket_receive(owned->platform_socket, writable, capacity);
}

static int elf_socket_send_to(int socket, const void* data, uint32_t size, const tabos_elf_socket_endpoint_t* endpoint)
{
    elf_socket_t* owned  = elf_socket(platform_riscv32_current_user_data(), socket);
    const void* readable = platform_executable_data_pointer(data, size);
    platform_network_address_t address;
    uint16_t port;
    if (owned == NULL) {
        return -TABOS_EBADF;
    }
    if (readable == NULL || size == 0U || size > TABOS_NETWORK_IO_MAX ||
        !elf_socket_endpoint(endpoint, &address, &port)) {
        return -TABOS_EINVAL;
    }
    return platform_network_socket_send_to(owned->platform_socket, readable, size, &address, port);
}

static int elf_socket_receive_from(int socket, void* data, uint32_t capacity, tabos_elf_socket_endpoint_t* peer)
{
    elf_socket_t* owned = elf_socket(platform_riscv32_current_user_data(), socket);
    void* writable_data = (void*) platform_executable_data_pointer(data, capacity);
    tabos_elf_socket_endpoint_t* writable_peer =
        peer != NULL ? (tabos_elf_socket_endpoint_t*) platform_executable_data_pointer(peer, sizeof(*peer)) : NULL;
    if (owned == NULL) {
        return -TABOS_EBADF;
    }
    if (writable_data == NULL || capacity == 0U || capacity > TABOS_NETWORK_IO_MAX ||
        (peer != NULL && writable_peer == NULL)) {
        return -TABOS_EINVAL;
    }
    platform_network_address_t address;
    uint16_t port;
    const int received =
        platform_network_socket_receive_from(owned->platform_socket, writable_data, capacity, &address, &port);
    if (received >= 0 && writable_peer != NULL) {
        elf_socket_write_endpoint(writable_peer, &address, port);
    }
    return received;
}

static int elf_wait_poll_device_subscription(loader_elf_application_t* application, uintptr_t parent,
                                             uint32_t requested_events, uint32_t* returned_events)
{
    const int pending = device_registry_event_pending(application, (tabos_device_subscription_t) parent);
    if (pending < 0) {
        return pending;
    }
    *returned_events = pending > 0 ? requested_events & (TABOS_WAIT_READABLE | TABOS_WAIT_STATE_CHANGED) : 0U;
    return 0;
}

static int elf_wait_prepare_socket(loader_elf_application_t* application, uintptr_t parent,
                                   platform_network_wait_item_t* item)
{
    elf_socket_t* socket = elf_socket(application, (int) parent);
    if (socket == NULL) {
        return -TABOS_EBADF;
    }
    item->socket = socket->platform_socket;
    return 0;
}

static const elf_wait_source_adapter_t elf_wait_source_adapters[ELF_WAIT_SOURCE_TYPE_COUNT] = {
    [ELF_WAIT_SOURCE_SOCKET] =
        {
                                  .valid_events = TABOS_WAIT_READABLE | TABOS_WAIT_WRITABLE | TABOS_WAIT_ERROR | TABOS_WAIT_HANGUP,
                                  .socket       = elf_wait_prepare_socket,
                                  },
    [ELF_WAIT_SOURCE_DEVICE_SUBSCRIPTION] =
        {
                                  .valid_events = TABOS_WAIT_READABLE | TABOS_WAIT_STATE_CHANGED | TABOS_WAIT_ERROR | TABOS_WAIT_HANGUP,
                                  .poll         = elf_wait_poll_device_subscription,
                                  },
};

static int elf_wait_sources(loader_elf_application_t* application, tabos_elf_wait_item_t* items, uint32_t count,
                            uint32_t timeout_ms)
{
    enum {
        ELF_WAIT_POLL_SLICE_MS = 10U,
    };
    platform_network_wait_item_t socket_items[TABOS_SOCKET_MAX];
    uint32_t socket_item_indices[TABOS_SOCKET_MAX];
    uint32_t socket_count = 0U;
    bool requires_polling = false;
    for (uint32_t index = 0U; index < count; ++index) {
        elf_wait_source_t* source = elf_wait_source(application, items[index].source);
        if (source == NULL || source->type >= ELF_WAIT_SOURCE_TYPE_COUNT) {
            return -TABOS_EBADF;
        }
        const elf_wait_source_adapter_t* adapter = &elf_wait_source_adapters[source->type];
        if (items[index].events == 0U || (items[index].events & ~adapter->valid_events) != 0U) {
            return -TABOS_EINVAL;
        }
        if (adapter->socket != NULL) {
            if (socket_count >= TABOS_SOCKET_MAX) {
                return -TABOS_EINVAL;
            }
            socket_items[socket_count] = (platform_network_wait_item_t) {
                .events = items[index].events,
            };
            const int prepared = adapter->socket(application, source->parent, &socket_items[socket_count]);
            if (prepared < 0) {
                return prepared;
            }
            socket_item_indices[socket_count++] = index;
        } else if (adapter->poll != NULL) {
            requires_polling = true;
        } else {
            return -TABOS_EINVAL;
        }
    }

    const uint64_t started_ms = platform_time_ms();
    while (true) {
        int ready = 0;
        for (uint32_t index = 0U; index < count; ++index) {
            items[index].returned_events = 0U;
            elf_wait_source_t* source    = elf_wait_source(application, items[index].source);
            if (source == NULL || source->type >= ELF_WAIT_SOURCE_TYPE_COUNT) {
                return -TABOS_EBADF;
            }
            const elf_wait_source_adapter_t* adapter = &elf_wait_source_adapters[source->type];
            if (adapter->poll != NULL) {
                const int polled =
                    adapter->poll(application, source->parent, items[index].events, &items[index].returned_events);
                if (polled < 0) {
                    return polled;
                }
                if (items[index].returned_events != 0U) {
                    ++ready;
                }
            }
        }

        uint32_t socket_timeout = 0U;
        if (ready == 0 && timeout_ms != 0U) {
            if (!requires_polling) {
                socket_timeout = timeout_ms;
            } else if (timeout_ms == TABOS_WAIT_TIMEOUT_INFINITE) {
                socket_timeout = ELF_WAIT_POLL_SLICE_MS;
            } else {
                const uint64_t elapsed = platform_time_ms() - started_ms;
                if (elapsed >= timeout_ms) {
                    return 0;
                }
                const uint32_t remaining = timeout_ms - (uint32_t) elapsed;
                socket_timeout           = remaining < ELF_WAIT_POLL_SLICE_MS ? remaining : ELF_WAIT_POLL_SLICE_MS;
            }
        }

        if (socket_count > 0U || (ready == 0 && requires_polling && timeout_ms != 0U)) {
            for (uint32_t index = 0U; index < socket_count; ++index) {
                socket_items[index].returned_events = 0U;
            }
            const int socket_ready =
                platform_network_socket_wait(socket_count > 0U ? socket_items : NULL, socket_count, socket_timeout);
            if (socket_ready < 0) {
                return socket_ready;
            }
            for (uint32_t index = 0U; index < socket_count; ++index) {
                tabos_elf_wait_item_t* item = &items[socket_item_indices[index]];
                item->returned_events       = socket_items[index].returned_events;
                if (item->returned_events != 0U) {
                    ++ready;
                }
            }
        }

        if (ready > 0 || timeout_ms == 0U || !requires_polling) {
            return ready;
        }
        if (atomic_load_explicit(&application->wait_cancel_requested, memory_order_acquire)) {
            return -TABOS_ECANCELED;
        }
    }
}

static int elf_wait(tabos_elf_wait_item_t* items, uint32_t count, uint32_t timeout_ms)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    tabos_elf_wait_item_t* writable =
        (tabos_elf_wait_item_t*) platform_executable_data_pointer(items, count * sizeof(*items));
    if (application == NULL || writable == NULL || count == 0U || count > TABOS_WAIT_MAX) {
        return -TABOS_EINVAL;
    }
    atomic_store_explicit(&application->wait_active, true, memory_order_release);
    int result;
    if (atomic_load_explicit(&application->wait_cancel_requested, memory_order_acquire)) {
        result = -TABOS_ECANCELED;
    } else {
        result = elf_wait_sources(application, writable, count, timeout_ms);
    }
    atomic_store_explicit(&application->wait_active, false, memory_order_release);
    return atomic_load_explicit(&application->wait_cancel_requested, memory_order_acquire) ? -TABOS_ECANCELED : result;
}

static elf_tls_t* elf_tls(loader_elf_application_t* application, int connection)
{
    if (application == NULL || connection <= 0) {
        return NULL;
    }
    const uint32_t handle     = (uint32_t) connection;
    const uint32_t index      = handle & ELF_TLS_INDEX_MASK;
    const uint32_t generation = handle >> ELF_TLS_INDEX_BITS;
    elf_tls_t* owned          = &application->tls[index];
    if (!owned->open || generation == 0U || owned->generation != generation) {
        return NULL;
    }
    return owned;
}

static int elf_tls_allocate(loader_elf_application_t* application, int platform_connection)
{
    for (int index = 0; index < ELF_TLS_CAPACITY; ++index) {
        if (!application->tls[index].open) {
            uint32_t generation = application->tls[index].generation + 1U;
            if (generation == 0U || generation > ELF_TLS_GENERATION_MAX) {
                generation = 1U;
            }
            application->tls[index] = (elf_tls_t) {
                .platform_connection = platform_connection,
                .generation          = generation,
                .open                = true,
            };
            return (int) ((generation << ELF_TLS_INDEX_BITS) | (uint32_t) index);
        }
    }
    (void) platform_tls_close(platform_connection);
    return -TABOS_EMFILE;
}

static int elf_tls_connect(const char* hostname, uint32_t port)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    const char* readable                  = platform_executable_data_pointer(hostname, 254U);
    if (application == NULL || readable == NULL || port == 0U || port > UINT16_MAX || strnlen(readable, 254U) == 254U) {
        return -TABOS_EINVAL;
    }
    const int platform_connection = platform_tls_connect(readable, (uint16_t) port);
    return platform_connection < 0 ? platform_connection : elf_tls_allocate(application, platform_connection);
}

static int elf_tls_close(int connection)
{
    elf_tls_t* owned = elf_tls(platform_riscv32_current_user_data(), connection);
    if (owned == NULL) {
        return -TABOS_EBADF;
    }
    const uint32_t generation = owned->generation;
    const int result          = platform_tls_close(owned->platform_connection);
    *owned                    = (elf_tls_t) {.generation = generation};
    return result;
}

static int elf_tls_send(int connection, const void* data, uint32_t size)
{
    elf_tls_t* owned     = elf_tls(platform_riscv32_current_user_data(), connection);
    const void* readable = platform_executable_data_pointer(data, size);
    if (owned == NULL) {
        return -TABOS_EBADF;
    }
    if (readable == NULL || size == 0U || size > TABOS_NETWORK_IO_MAX) {
        return -TABOS_EINVAL;
    }
    return platform_tls_send(owned->platform_connection, readable, size);
}

static int elf_tls_receive(int connection, void* data, uint32_t capacity)
{
    elf_tls_t* owned = elf_tls(platform_riscv32_current_user_data(), connection);
    void* writable   = (void*) platform_executable_data_pointer(data, capacity);
    if (owned == NULL) {
        return -TABOS_EBADF;
    }
    if (writable == NULL || capacity == 0U || capacity > TABOS_NETWORK_IO_MAX) {
        return -TABOS_EINVAL;
    }
    return platform_tls_receive(owned->platform_connection, writable, capacity);
}

static int elf_system_info(tabos_elf_system_info_t* info)
{
    if (info == NULL) {
        return -TABOS_EINVAL;
    }
    platform_diagnostics_t diagnostics;
    if (!platform_get_diagnostics(&diagnostics)) {
        return -TABOS_EIO;
    }
    memset(info, 0, sizeof(*info));
    (void) snprintf(info->target, sizeof(info->target), "%s", platform_name());
    (void) snprintf(info->device, sizeof(info->device), "%s",
                    diagnostics.device_name != NULL ? diagnostics.device_name : "Unknown");
    (void) snprintf(info->display, sizeof(info->display), "%s", platform_display_name());
    info->display_width              = TABOS_DISPLAY_WIDTH;
    info->display_height             = TABOS_DISPLAY_HEIGHT;
    info->cpu_cores                  = diagnostics.cpu_cores;
    info->cpu_frequency_mhz          = diagnostics.cpu_frequency_mhz;
    info->memory_total_low           = (uint32_t) diagnostics.memory_total_bytes;
    info->memory_total_high          = (uint32_t) (diagnostics.memory_total_bytes >> 32U);
    info->external_memory_total_low  = (uint32_t) diagnostics.external_memory_total_bytes;
    info->external_memory_total_high = (uint32_t) (diagnostics.external_memory_total_bytes >> 32U);
    return 0;
}

static uint32_t elf_device_count(void)
{
    return (uint32_t) device_registry_count();
}

static int elf_device_at(uint32_t index, tabos_device_info_t* info)
{
    tabos_device_info_t* writable = (tabos_device_info_t*) platform_executable_data_pointer(info, sizeof(*info));
    if (writable == NULL) {
        return -TABOS_EINVAL;
    }
    return device_registry_at(index, writable) ? 0 : -TABOS_ENOENT;
}

static int elf_device_get(tabos_device_id_t id, tabos_device_info_t* info)
{
    tabos_device_info_t* writable = (tabos_device_info_t*) platform_executable_data_pointer(info, sizeof(*info));
    if (writable == NULL) {
        return -TABOS_EINVAL;
    }
    return device_registry_get(id, writable) ? 0 : -TABOS_ENOENT;
}

static int elf_device_find(const char* name, tabos_device_info_t* info)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    const char* readable                  = platform_executable_data_pointer(name, TABOS_DEVICE_NAME_MAX + 1U);
    tabos_device_info_t* writable = (tabos_device_info_t*) platform_executable_data_pointer(info, sizeof(*info));
    if (application == NULL || readable == NULL || writable == NULL ||
        memchr(readable, '\0', TABOS_DEVICE_NAME_MAX + 1U) == NULL) {
        return -TABOS_EINVAL;
    }
    return device_registry_find(readable, writable) ? 0 : -TABOS_ENOENT;
}

static int elf_device_subscribe(void)
{
    loader_elf_application_t* application          = platform_riscv32_current_user_data();
    const tabos_device_subscription_t subscription = device_registry_subscribe(application);
    if (subscription == TABOS_DEVICE_SUBSCRIPTION_INVALID) {
        return -TABOS_ENFILE;
    }
    const tabos_wait_source_t source =
        elf_wait_source_allocate(application, ELF_WAIT_SOURCE_DEVICE_SUBSCRIPTION, (uintptr_t) subscription);
    if (source == TABOS_WAIT_SOURCE_INVALID) {
        (void) device_registry_unsubscribe(application, subscription);
        return -TABOS_ENFILE;
    }
    return subscription;
}

static int elf_device_subscription_close(int subscription)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (!device_registry_unsubscribe(application, subscription)) {
        return -TABOS_EBADF;
    }
    elf_wait_source_invalidate(
        application, elf_wait_source_find(application, ELF_WAIT_SOURCE_DEVICE_SUBSCRIPTION, (uintptr_t) subscription));
    return 0;
}

static int elf_device_subscription_wait_source(int subscription)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (device_registry_event_pending(application, subscription) < 0) {
        return -TABOS_EBADF;
    }
    const tabos_wait_source_t source =
        elf_wait_source_find(application, ELF_WAIT_SOURCE_DEVICE_SUBSCRIPTION, (uintptr_t) subscription);
    return source != TABOS_WAIT_SOURCE_INVALID ? source : -TABOS_EBADF;
}

static int elf_device_event_read(int subscription, tabos_device_event_t* event)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    tabos_device_event_t* writable = (tabos_device_event_t*) platform_executable_data_pointer(event, sizeof(*event));
    return writable != NULL ? device_registry_read_event(application, subscription, writable) : -TABOS_EINVAL;
}

static int elf_battery_status(tabos_elf_battery_status_t* info)
{
    if (info == NULL) {
        return -TABOS_EINVAL;
    }
    tabos_elf_battery_status_t* writable =
        (tabos_elf_battery_status_t*) platform_executable_data_pointer(info, sizeof(*info));
    platform_battery_status_t source;
    if (writable == NULL || !platform_battery_status(&source)) {
        return -TABOS_EIO;
    }
    *writable = (tabos_elf_battery_status_t) {
        .available             = source.available ? 1U : 0U,
        .charging_enabled      = source.charging_enabled ? 1U : 0U,
        .fast_charging_enabled = source.fast_charging_enabled ? 1U : 0U,
        .voltage_mv            = source.voltage_mv,
        .current_ma            = source.current_ma,
        .power_mw              = source.power_mw,
        .percentage            = source.percentage,
        .charge_state          = source.charge_state,
    };
    return 0;
}

static int elf_battery_set_charging(uint32_t enabled)
{
    return platform_battery_set_charging(enabled != 0U) ? 0 : -TABOS_EIO;
}

static int elf_battery_set_fast_charging(uint32_t enabled)
{
    return platform_battery_set_fast_charging(enabled != 0U) ? 0 : -TABOS_EIO;
}

static int elf_graphics_open(uint32_t* width, uint32_t* height)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    platform_framebuffer_t* framebuffer   = display_framebuffer();
    if (application == NULL || framebuffer == NULL || width == NULL || height == NULL) {
        return -TABOS_EINVAL;
    }
    if (!platform_graphics_begin()) {
        return -TABOS_EIO;
    }
    application->graphics_active        = true;
    application->graphics_overlay_flags = TABOS_GRAPHICS_OVERLAY_ALL;
    display_overlay_set_flags(application->graphics_overlay_flags);
    console_set_graphics_active(true);
    *width  = (uint32_t) framebuffer->width;
    *height = (uint32_t) framebuffer->height;
    return 0;
}

static bool elf_graphics_execute_one(loader_elf_application_t* application)
{
    if (application == NULL || application->graphics_command_count == 0U) {
        return true;
    }
    platform_framebuffer_t* framebuffer = display_framebuffer();
    if (framebuffer == NULL) {
        return false;
    }
    elf_graphics_command_t* command = &application->graphics_commands[application->graphics_command_head];
    bool result                     = true;
    if (command->type == ELF_GRAPHICS_FILL) {
        if (!platform_graphics_fill(framebuffer, command->data.fill.x, command->data.fill.y, command->data.fill.width,
                                    command->data.fill.height, command->data.fill.color)) {
            raster_fill(framebuffer, command->data.fill.x, command->data.fill.y, command->data.fill.width,
                        command->data.fill.height, command->data.fill.color);
        }
    } else {
        result =
            platform_graphics_blit(framebuffer, &command->data.blit) || raster_blit(framebuffer, &command->data.blit);
    }
    application->graphics_command_head = (application->graphics_command_head + 1U) % ELF_GRAPHICS_COMMAND_CAPACITY;
    --application->graphics_command_count;
    return result;
}

static bool elf_graphics_drain(loader_elf_application_t* application)
{
    bool result = true;
    while (application != NULL && application->graphics_command_count != 0U) {
        if (!elf_graphics_execute_one(application)) {
            result = false;
        }
    }
    return result;
}

static bool elf_graphics_enqueue(loader_elf_application_t* application, const elf_graphics_command_t* command)
{
    if (application == NULL || command == NULL) {
        return false;
    }
    if (application->graphics_command_count == ELF_GRAPHICS_COMMAND_CAPACITY &&
        !elf_graphics_execute_one(application)) {
        return false;
    }
    const size_t tail =
        (application->graphics_command_head + application->graphics_command_count) % ELF_GRAPHICS_COMMAND_CAPACITY;
    application->graphics_commands[tail] = *command;
    ++application->graphics_command_count;
    return true;
}

static int elf_graphics_clear(uint32_t color)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    platform_framebuffer_t* framebuffer   = display_framebuffer();
    if (application == NULL || !application->graphics_active || framebuffer == NULL) {
        return -TABOS_EINVAL;
    }
    const elf_graphics_command_t command = {
        .type      = ELF_GRAPHICS_FILL,
        .data.fill = {.x      = 0,
                      .y      = 0,
                      .width  = (uint32_t) framebuffer->width,
                      .height = (uint32_t) framebuffer->height,
                      .color  = (tabos_color_t) color},
    };
    return elf_graphics_enqueue(application, &command) ? 0 : -TABOS_EIO;
}

static int elf_graphics_fill_rect(int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    platform_framebuffer_t* framebuffer   = display_framebuffer();
    if (application == NULL || !application->graphics_active || framebuffer == NULL) {
        return -TABOS_EINVAL;
    }
    const elf_graphics_command_t command = {
        .type      = ELF_GRAPHICS_FILL,
        .data.fill = {.x = x, .y = y, .width = width, .height = height, .color = (tabos_color_t) color},
    };
    return elf_graphics_enqueue(application, &command) ? 0 : -TABOS_EIO;
}

static int elf_graphics_blit(int32_t x, int32_t y, uint32_t width, uint32_t height, const uint16_t* pixels)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    platform_framebuffer_t* framebuffer   = display_framebuffer();
    const size_t pixel_bytes = width <= SIZE_MAX / height && (size_t) width * height <= SIZE_MAX / sizeof(*pixels) ?
                                   (size_t) width * height * sizeof(*pixels) :
                                   0U;
    const uint16_t* source   = platform_executable_data_pointer(pixels, pixel_bytes);
    if (application == NULL || !application->graphics_active || framebuffer == NULL || source == NULL || width == 0U ||
        height == 0U || width > SIZE_MAX / height) {
        return -TABOS_EINVAL;
    }
    const elf_graphics_command_t command = {
        .type = ELF_GRAPHICS_BLIT,
        .data.blit =
            {
                        .pixels        = source,
                        .bitmap_width  = width,
                        .bitmap_height = height,
                        .source        = {.x = 0, .y = 0, .width = width, .height = height},
                        .destination   = {.x = x, .y = y, .width = width, .height = height},
                        .rotation      = TABOS_GRAPHICS_ROTATE_0,
                        .opacity       = 255U,
                        },
    };
    return elf_graphics_enqueue(application, &command) ? 0 : -TABOS_EIO;
}

static uint32_t elf_graphics_capabilities(void)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || !application->graphics_active) {
        return 0U;
    }
    return TABOS_GRAPHICS_CAP_QUEUED | TABOS_GRAPHICS_CAP_TRANSFORM | TABOS_GRAPHICS_CAP_BLEND |
           TABOS_GRAPHICS_CAP_COLOR_KEY | platform_graphics_capabilities();
}

static int elf_graphics_blit_ex(const tabos_graphics_blit_options_t* options)
{
    loader_elf_application_t* application         = platform_riscv32_current_user_data();
    const tabos_graphics_blit_options_t* readable = platform_executable_data_pointer(options, sizeof(*options));
    if (application == NULL || !application->graphics_active || readable == NULL || readable->bitmap_width == 0U ||
        readable->bitmap_height == 0U || readable->bitmap_width > SIZE_MAX / readable->bitmap_height ||
        (size_t) readable->bitmap_width * readable->bitmap_height > SIZE_MAX / sizeof(*readable->pixels)) {
        return -TABOS_EINVAL;
    }
    tabos_graphics_blit_options_t copied = *readable;
    const size_t bytes                   = (size_t) copied.bitmap_width * copied.bitmap_height * sizeof(*copied.pixels);
    copied.pixels                        = platform_executable_data_pointer(copied.pixels, bytes);
    if (copied.pixels == NULL) {
        return -TABOS_EINVAL;
    }
    const elf_graphics_command_t command = {
        .type      = ELF_GRAPHICS_BLIT,
        .data.blit = copied,
    };
    return elf_graphics_enqueue(application, &command) ? 0 : -TABOS_EIO;
}

static int elf_graphics_present(void)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    return application != NULL && application->graphics_active && elf_graphics_drain(application) &&
                   display_graphics_present() ?
               0 :
               -TABOS_EIO;
}

static int elf_graphics_close(void)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || !application->graphics_active) {
        return -TABOS_EINVAL;
    }
    if (!elf_graphics_drain(application) || !display_graphics_present()) {
        return -TABOS_EIO;
    }
    application->graphics_active        = false;
    application->graphics_overlay_flags = TABOS_GRAPHICS_OVERLAY_ALL;
    display_overlay_set_flags(TABOS_GRAPHICS_OVERLAY_ALL);
    platform_graphics_end();
    console_set_graphics_active(false);
    return 0;
}

static int elf_graphics_set_overlays(uint32_t flags)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || !application->graphics_active ||
        (flags & ~(uint32_t) TABOS_GRAPHICS_OVERLAY_ALL) != 0U) {
        return -TABOS_EINVAL;
    }
    application->graphics_overlay_flags = flags;
    display_overlay_set_flags(flags);
    return 0;
}

static int elf_exec(const char* path, uint32_t argc, const char* const* argv)
{
    loader_elf_application_t* application = platform_riscv32_current_user_data();
    if (application == NULL || path == NULL || path[0] == '\0') {
        return -TABOS_EINVAL;
    }
    if (atomic_exchange_explicit(&application->exec_status_ready, false, memory_order_acq_rel)) {
        const int status = atomic_load_explicit(&application->exec_status, memory_order_acquire);
        atomic_store_explicit(&application->exec_in_flight, false, memory_order_release);
        return status;
    }
    if (atomic_load_explicit(&application->exec_in_flight, memory_order_acquire)) {
        return TABOS_ELF_EXEC_PENDING;
    }
    const char* readable_path = platform_executable_data_pointer(path, TABOS_FS_PATH_MAX);
    if (readable_path == NULL) {
        return -TABOS_EIO;
    }
    const size_t length = strlen(readable_path);
    if (length >= sizeof(application->exec_path)) {
        return -TABOS_ENAMETOOLONG;
    }
    if (!copy_arguments((size_t) argc, argv, application->exec_argument_data, sizeof(application->exec_argument_data),
                        application->exec_argv)) {
        return -TABOS_EINVAL;
    }
    memcpy(application->exec_path, readable_path, length + 1U);
    application->exec_argc = (size_t) argc;
    atomic_store_explicit(&application->exec_in_flight, true, memory_order_release);
    atomic_store_explicit(&application->exec_requested, true, memory_order_release);
    return TABOS_ELF_EXEC_PENDING;
}

static void elf_yield(void)
{
    platform_input_wait();
}

static bool elf_entry(tabos_app_context_t* context)
{
    loader_elf_application_t* application = application_from_context(context);
    if (application == NULL) {
        return false;
    }
    application->context = context;
    application->console = tabos_app_console(context);
    if (application->console == NULL) {
        return false;
    }

    const loader_elf_result_t result = loader_elf_load_file(application->path, &application->image);
    if (result != LOADER_ELF_OK) {
        char message[96];
        (void) snprintf(message, sizeof(message), "ELF load FAILED: %s", loader_elf_result_name(result));
        platform_log(message);
        (void) tabos_console_write(application->console, "ELF load FAILED: ");
        (void) tabos_console_write_line(application->console, loader_elf_result_name(result));
        return false;
    }
    application->heap_limit = application->image.info.requested_heap_bytes;
    if (!platform_can_execute_riscv32()) {
        (void) tabos_console_write_line(application->console, "ELF execution unsupported on this target");
        return false;
    }

    char load_message[640];
    (void) snprintf(load_message, sizeof(load_message), "ELF image loaded from %s: %u memory bytes", application->path,
                    (unsigned int) application->image.memory_size);
    platform_log(load_message);

    const tabos_elf_api_t api = {
        .abi_version                     = TABOS_ELF_API_VERSION,
        .console_write                   = elf_console_write,
        .request_exit                    = elf_request_exit,
        .console_read                    = elf_console_read,
        .console_clear                   = elf_console_clear,
        .fs_getcwd                       = elf_fs_getcwd,
        .fs_chdir                        = elf_fs_chdir,
        .fs_list                         = elf_fs_list,
        .exec                            = elf_exec,
        .yield                           = elf_yield,
        .console_write_raw               = elf_console_write_raw,
        .fd_open                         = elf_fd_open,
        .fd_close                        = elf_fd_close,
        .fd_read                         = elf_fd_read,
        .fd_write                        = elf_fd_write,
        .fd_seek                         = elf_fd_seek,
        .fs_stat                         = elf_fs_stat_path,
        .fd_stat                         = elf_fd_stat,
        .fs_mkdir                        = elf_fs_mkdir_path,
        .fs_unlink                       = elf_fs_unlink_path,
        .fs_rename                       = elf_fs_rename_path,
        .fd_get_flags                    = elf_fd_get_flags,
        .fd_set_flags                    = elf_fd_set_flags,
        .heap_sbrk                       = elf_heap_sbrk,
        .fs_rmdir                        = elf_fs_rmdir_path,
        .monotonic_ms                    = elf_monotonic_ms,
        .system_info                     = elf_system_info,
        .graphics_open                   = elf_graphics_open,
        .graphics_clear                  = elf_graphics_clear,
        .graphics_fill_rect              = elf_graphics_fill_rect,
        .graphics_blit                   = elf_graphics_blit,
        .graphics_present                = elf_graphics_present,
        .graphics_close                  = elf_graphics_close,
        .graphics_capabilities           = elf_graphics_capabilities,
        .graphics_blit_ex                = elf_graphics_blit_ex,
        .tty_get_mode                    = elf_tty_get_mode,
        .tty_set_mode                    = elf_tty_set_mode,
        .input_poll                      = elf_input_poll,
        .wall_time_get                   = elf_wall_time_get,
        .wall_time_set                   = elf_wall_time_set,
        .system_action                   = elf_system_action,
        .network_status                  = elf_network_status,
        .network_connect_saved           = elf_network_connect_saved,
        .network_disconnect              = elf_network_disconnect,
        .network_resolve                 = elf_network_resolve,
        .network_echo                    = elf_network_echo,
        .socket_open                     = elf_socket_open,
        .socket_close                    = elf_socket_close,
        .socket_bind                     = elf_socket_bind,
        .socket_listen                   = elf_socket_listen,
        .socket_accept                   = elf_socket_accept,
        .socket_connect                  = elf_socket_connect,
        .socket_set_nonblocking          = elf_socket_set_nonblocking,
        .socket_shutdown                 = elf_socket_shutdown,
        .socket_send                     = elf_socket_send,
        .socket_receive                  = elf_socket_receive,
        .socket_send_to                  = elf_socket_send_to,
        .socket_receive_from             = elf_socket_receive_from,
        .socket_get_local_endpoint       = elf_socket_get_local_endpoint,
        .battery_status                  = elf_battery_status,
        .battery_set_charging            = elf_battery_set_charging,
        .battery_set_fast_charging       = elf_battery_set_fast_charging,
        .graphics_set_overlays           = elf_graphics_set_overlays,
        .tls_connect                     = elf_tls_connect,
        .tls_close                       = elf_tls_close,
        .tls_send                        = elf_tls_send,
        .tls_receive                     = elf_tls_receive,
        .device_count                    = elf_device_count,
        .device_at                       = elf_device_at,
        .device_get                      = elf_device_get,
        .device_find                     = elf_device_find,
        .device_subscribe                = elf_device_subscribe,
        .device_subscription_close       = elf_device_subscription_close,
        .device_event_read               = elf_device_event_read,
        .socket_wait_source              = elf_socket_wait_source,
        .device_subscription_wait_source = elf_device_subscription_wait_source,
        .wait                            = elf_wait,
    };
    application->execution = platform_riscv32_create(
        application->image.entry, application->image.memory, application->image.memory_size,
        application->image.info.minimum_address, application->image.info.requested_heap_bytes,
        application->image.info.requested_stack_bytes, &api, application->argc, application->argv, application);
    if (application->execution == NULL) {
        (void) tabos_console_write_line(application->console, "ELF execution FAILED");
        return false;
    }
    return true;
}

static void elf_update(tabos_app_context_t* context)
{
    loader_elf_application_t* application = application_from_context(context);
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
        const tabos_app_result_t launch_result =
            tabos_app_exec_args(context, application->exec_path, application->exec_argc, application->exec_argv);
        if (launch_result != TABOS_APP_RESULT_OK) {
            /* A child whose entry function fails is finished synchronously and
             * leaves its exit status on the parent. The launch error below is
             * the authoritative result for this exec request, so discard that
             * duplicate child status before it can satisfy the next request. */
            int discarded_status = 0;
            (void) tabos_app_take_child_status(context, &discarded_status);
            atomic_store_explicit(&application->exec_status, -(100 + (int) launch_result), memory_order_release);
            atomic_store_explicit(&application->exec_status_ready, true, memory_order_release);
        } else {
            return;
        }
    }
    if (atomic_exchange_explicit(&application->exit_requested, false, memory_order_acq_rel)) {
        tabos_app_request_exit(context,
                               atomic_load_explicit(&application->requested_exit_status, memory_order_acquire));
        return;
    }

    int returned_status = 0;
    const platform_riscv32_result_t result =
        platform_riscv32_step(application->execution, ELF_INSTRUCTIONS_PER_UPDATE, &returned_status);
    if (result == PLATFORM_RISCV32_YIELDED) {
        return;
    }
    if (result == PLATFORM_RISCV32_FAULT) {
        (void) tabos_console_write_line(application->console, "ELF execution FAILED");
        kernel_process_fail(context, TABOS_PROCESS_TERMINATION_FAULT, 5);
        return;
    }
    char exit_message[56];
    (void) snprintf(exit_message, sizeof(exit_message), "ELF entry returned status %d", returned_status);
    platform_log(exit_message);
    if (!atomic_load_explicit(&application->exit_requested, memory_order_acquire)) {
        kernel_process_fail(context, TABOS_PROCESS_TERMINATION_RETURN, returned_status);
    }
}

static bool elf_heap_guards_intact(const loader_elf_application_t* application)
{
    if (application->heap_allocation == NULL) {
        return true;
    }
    for (size_t index = 0U; index < ELF_HEAP_GUARD_SIZE; ++index) {
        if (application->heap_allocation[index] != ELF_HEAP_GUARD_VALUE ||
            application->heap_allocation[ELF_HEAP_GUARD_SIZE + application->heap_limit + index] !=
                ELF_HEAP_GUARD_VALUE) {
            return false;
        }
    }
    return true;
}

static void elf_cancel_wait(loader_elf_application_t* application)
{
    atomic_store_explicit(&application->wait_cancel_requested, true, memory_order_release);
    if (!atomic_load_explicit(&application->wait_active, memory_order_acquire)) {
        return;
    }
    for (size_t index = 0U; index < ELF_WAIT_SOURCE_CAPACITY; ++index) {
        const elf_wait_source_t* source = &application->wait_sources[index];
        if (!source->open || source->type != ELF_WAIT_SOURCE_SOCKET) {
            continue;
        }
        elf_socket_t* socket = elf_socket(application, (int) source->parent);
        if (socket != NULL) {
            platform_network_socket_interrupt(socket->platform_socket);
        }
    }
}

static void elf_release_resources(loader_elf_application_t* application)
{
    if (application == NULL) {
        return;
    }
    elf_cancel_wait(application);
    device_registry_unsubscribe_owner(application);
    for (size_t index = 0U; index < ELF_WAIT_SOURCE_CAPACITY; ++index) {
        if (application->wait_sources[index].open &&
            application->wait_sources[index].type == ELF_WAIT_SOURCE_DEVICE_SUBSCRIPTION) {
            const uint32_t generation        = application->wait_sources[index].generation;
            application->wait_sources[index] = (elf_wait_source_t) {.generation = generation};
        }
    }
    if (application->graphics_active) {
        (void) elf_graphics_drain(application);
        (void) display_graphics_present();
        application->graphics_active        = false;
        application->graphics_overlay_flags = TABOS_GRAPHICS_OVERLAY_ALL;
        display_overlay_set_flags(TABOS_GRAPHICS_OVERLAY_ALL);
        platform_graphics_end();
        console_set_graphics_active(false);
    }
    for (size_t index = 0U; index < ELF_SOCKET_CAPACITY; ++index) {
        if (application->sockets[index].open) {
            platform_network_socket_interrupt(application->sockets[index].platform_socket);
        }
    }
    const bool socket_operations_suspended = platform_network_socket_operations_suspend();
    if (!socket_operations_suspended) {
        platform_log("Could not suspend socket operations during process cleanup");
    }
    for (size_t index = 0U; index < ELF_SOCKET_CAPACITY; ++index) {
        if (application->sockets[index].open) {
            platform_network_socket_dispose(application->sockets[index].platform_socket);
            elf_wait_source_invalidate(application, application->sockets[index].wait_source);
            application->sockets[index] = (elf_socket_t) {0};
        }
    }
    for (size_t index = 0U; index < ELF_TLS_CAPACITY; ++index) {
        if (application->tls[index].open) {
            (void) platform_tls_close(application->tls[index].platform_connection);
            application->tls[index] = (elf_tls_t) {0};
        }
    }
    platform_riscv32_destroy(application->execution);
    application->execution = NULL;
    if (socket_operations_suspended) {
        platform_network_socket_operations_resume();
    }
    loader_elf_unload(&application->image);
    for (size_t index = 3U; index < ELF_DESCRIPTOR_CAPACITY; ++index) {
        if (application->descriptors[index].open) {
            (void) tabos_fs_close(application->descriptors[index].kernel_descriptor);
            application->descriptors[index] = (elf_descriptor_t) {0};
        }
    }
    if (!elf_heap_guards_intact(application)) {
        platform_log("ELF heap boundary corruption detected during process cleanup");
    }
    free(application->heap_allocation);
    application->heap_allocation = NULL;
    application->heap            = NULL;
    application->heap_used       = 0U;
    application->heap_limit      = 0U;
    application->context         = NULL;
    application->console         = NULL;
}

static void elf_cleanup(tabos_app_context_t* context, int exit_status)
{
    (void) exit_status;
    elf_release_resources(application_from_context(context));
}

loader_elf_application_t* loader_elf_application_create(const char* path, size_t argc, const char* const* argv)
{
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }
    const size_t path_length = strlen(path);
    if (path_length >= TABOS_FS_PATH_MAX) {
        return NULL;
    }

    loader_elf_application_t* application = calloc(1U, sizeof(*application));
    if (application == NULL) {
        return NULL;
    }
    if (!copy_arguments(argc, argv, application->argument_data, sizeof(application->argument_data),
                        application->argv)) {
        free(application);
        return NULL;
    }
    application->argc = argc;
    memcpy(application->path, path, path_length + 1U);
    for (size_t index = 0U; index < 3U; ++index) {
        application->descriptors[index] = (elf_descriptor_t) {.flags = 0, .open = true};
    }
    if (tabos_fs_getcwd(application->working_directory, sizeof(application->working_directory)) == NULL) {
        memcpy(application->working_directory, "A:/", 4U);
    }

    const char* name = strrchr(path, '/');
    name             = name != NULL ? name + 1 : path;
    if (name[0] == '\0' || strlen(name) > TABOS_FS_NAME_MAX) {
        free(application);
        return NULL;
    }
    memcpy(application->name, name, strlen(name) + 1U);
    application->descriptor = (tabos_app_descriptor_t) {
        .abi_version  = TABOS_APPLICATION_ABI_VERSION,
        .name         = application->name,
        .version      = "ELF",
        .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
        .entry        = elf_entry,
        .update       = elf_update,
        .cleanup      = elf_cleanup,
    };
    return application;
}

const tabos_app_descriptor_t* loader_elf_application_descriptor(const loader_elf_application_t* application)
{
    return application != NULL ? &application->descriptor : NULL;
}

void loader_elf_application_destroy(void* application)
{
    loader_elf_application_t* elf_application = application;
    elf_release_resources(elf_application);
    free(elf_application);
}

const char* loader_elf_application_working_directory(const loader_elf_application_t* application)
{
    return application != NULL ? application->working_directory : NULL;
}

bool loader_elf_application_set_working_directory(loader_elf_application_t* application, const char* working_directory)
{
    if (application == NULL || working_directory == NULL ||
        strlen(working_directory) >= sizeof(application->working_directory)) {
        return false;
    }
    memcpy(application->working_directory, working_directory, strlen(working_directory) + 1U);
    return true;
}

uint32_t loader_elf_application_tty_mode(const loader_elf_application_t* application)
{
    return application != NULL ? application->tty_mode : 0U;
}

bool loader_elf_application_set_tty_mode(loader_elf_application_t* application, uint32_t mode)
{
    if (application == NULL || (mode & ~(uint32_t) (TABOS_TTY_MODE_SCROLL_KEYS | TABOS_TTY_MODE_RAW_INPUT)) != 0U) {
        return false;
    }
    application->tty_mode = mode;
    return true;
}
