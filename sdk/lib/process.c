#include <tabos/process.h>

#include <tabos/elf_api.h>
#include <tabos/posix_compat.h>

extern const tabos_elf_api_t *tabos_runtime_api;

int tabos_exec(const char *path, int argc, const char *const argv[])
{
    const tabos_elf_api_t *api = tabos_runtime_api;
    if (api == 0 || api->exec == 0 || api->yield == 0 || path == 0 || path[0] == '\0' ||
        argc < 1 || argc > TABOS_ELF_ARG_MAX || argv == 0) {
        return -TABOS_EINVAL;
    }

    int status = TABOS_ELF_EXEC_PENDING;
    while (status == TABOS_ELF_EXEC_PENDING) {
        status = api->exec(path, (uint32_t)argc, argv);
        if (status == TABOS_ELF_EXEC_PENDING) api->yield();
    }
    return status;
}
