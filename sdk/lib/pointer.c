#include <tabos/internal/elf_api.h>
#include <tabos/pointer.h>

#include <errno.h>

extern const tabos_elf_api_t* tabos_runtime_api;

static int api_result(int result)
{
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return result;
}

tabos_pointer_stream_t tabos_pointer_open(tabos_device_id_t device_id)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->pointer_open == NULL) {
        errno = ENOSYS;
        return TABOS_POINTER_STREAM_INVALID;
    }
    const int result = tabos_runtime_api->pointer_open(device_id);
    if (result < 0) {
        errno = -result;
        return TABOS_POINTER_STREAM_INVALID;
    }
    return result;
}

int tabos_pointer_close(tabos_pointer_stream_t stream)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->pointer_close == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->pointer_close(stream));
}

int tabos_pointer_read(tabos_pointer_stream_t stream, tabos_pointer_event_t* event)
{
    if (event == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->pointer_read == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->pointer_read(stream, event));
}
