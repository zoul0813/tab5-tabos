#include <tabos/internal/elf_api.h>
#include <tabos/filesystem.h>
#include <tabos/pointer.h>
#include <tabos/wait.h>

#include <errno.h>

const tabos_elf_api_t* tabos_runtime_api;

static int open_call(tabos_device_id_t device_id)
{
    return device_id == 7U ? 11 : -TABOS_ENODEV;
}

static int close_call(int stream)
{
    return stream == 11 ? 0 : -TABOS_EBADF;
}

static int read_call(int stream, tabos_pointer_event_t* event)
{
    if (stream != 11) {
        return -TABOS_EBADF;
    }
    *event = (tabos_pointer_event_t) {.type = TABOS_POINTER_DOWN, .device_id = 7U, .x = 3, .y = 4};
    return 0;
}

static int wait_source_call(int stream)
{
    return stream == 11 ? 13 : -TABOS_EBADF;
}

int main(void)
{
    static const tabos_elf_api_t api = {
        .pointer_open        = open_call,
        .pointer_close       = close_call,
        .pointer_read        = read_call,
        .pointer_wait_source = wait_source_call,
    };
    tabos_runtime_api = &api;
    tabos_pointer_event_t event;
    if (tabos_pointer_open(7U) != 11 || tabos_pointer_wait_source(11) != 13 || tabos_pointer_read(11, &event) != 0 ||
        event.type != TABOS_POINTER_DOWN || event.x != 3 || event.y != 4 || tabos_pointer_close(11) != 0) {
        return 1;
    }
    errno = 0;
    if (tabos_pointer_read(99, &event) == 0 || errno != EBADF ||
        tabos_pointer_wait_source(99) != TABOS_WAIT_SOURCE_INVALID || errno != EBADF) {
        return 1;
    }
    tabos_runtime_api = NULL;
    errno             = 0;
    return tabos_pointer_open(7U) == TABOS_POINTER_STREAM_INVALID && errno == ENOSYS ? 0 : 1;
}
