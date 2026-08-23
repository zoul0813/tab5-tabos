#include <tabos/input.h>
#include <tabos/internal/elf_api.h>

#include <errno.h>
#include <sched.h>
#include <stddef.h>

extern const tabos_elf_api_t* tabos_runtime_api;

bool tabos_input_poll(tabos_input_event_t* event)
{
    if (event == NULL || tabos_runtime_api == NULL || tabos_runtime_api->input_poll == NULL) {
        errno = event == NULL ? EINVAL : ENOSYS;
        return false;
    }
    const int result = tabos_runtime_api->input_poll(event);
    if (result < 0) {
        errno = -result;
    }
    return result > 0;
}

bool tabos_input_wait(tabos_input_event_t* event)
{
    if (event == NULL || tabos_runtime_api == NULL || tabos_runtime_api->input_poll == NULL) {
        errno = event == NULL ? EINVAL : ENOSYS;
        return false;
    }
    for (;;) {
        const int result = tabos_runtime_api->input_poll(event);
        if (result > 0) {
            return true;
        }
        if (result < 0) {
            errno = -result;
            return false;
        }
        if (sched_yield() != 0) {
            return false;
        }
    }
}
