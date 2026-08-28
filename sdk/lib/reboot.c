#include <tabos/internal/elf_api.h>

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <sys/reboot.h>

extern const tabos_elf_api_t* tabos_runtime_api;

int reboot(int command)
{
    uint32_t action = 0U;
    if (command == RB_AUTOBOOT) {
        action = TABOS_ELF_SYSTEM_REBOOT;
    } else if (command == RB_POWER_OFF) {
        action = TABOS_ELF_SYSTEM_POWER_OFF;
    } else {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->system_action == NULL) {
        errno = ENOSYS;
        return -1;
    }
    (void) fflush(NULL);
    const int result = tabos_runtime_api->system_action(action);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    for (;;) {
        (void) sched_yield();
    }
}
