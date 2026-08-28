#include <tabos/internal/elf_api.h>
#include <tabos/posix_compat.h>

#include <errno.h>
#include <sys/reboot.h>

const tabos_elf_api_t* tabos_runtime_api;

static int reject_action(uint32_t action)
{
    (void) action;
    return -TABOS_EBUSY;
}

int main(void)
{
    if (reboot(0) == 0 || errno != EINVAL) {
        return 1;
    }
    if (reboot(RB_AUTOBOOT) == 0 || errno != ENOSYS) {
        return 1;
    }
    const tabos_elf_api_t api = {
        .system_action = reject_action,
    };
    tabos_runtime_api = &api;
    if (reboot(RB_POWER_OFF) == 0 || errno != TABOS_EBUSY) {
        return 1;
    }
    return 0;
}
