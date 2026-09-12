#include <tabos/process.h>

#include <tabos/internal/elf_api.h>
#include <tabos/posix_compat.h>
#include <errno.h>
#include <sched.h>

extern const tabos_elf_api_t* tabos_runtime_api;

tabos_wait_source_t tabos_process_wait_source(int pid)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->process_wait_source == NULL) {
        errno = ENOSYS;
        return TABOS_WAIT_SOURCE_INVALID;
    }
    const int result = tabos_runtime_api->process_wait_source(pid);
    if (result < 0) {
        errno = -result;
        return TABOS_WAIT_SOURCE_INVALID;
    }
    return result;
}

int tabos_exec(const char* path, int argc, const char* const argv[])
{
    const tabos_elf_api_t* api = tabos_runtime_api;
    if (api == 0 || api->exec == 0 || api->yield == 0 || path == 0 || path[0] == '\0' || argc < 1 ||
        argc > TABOS_ELF_ARG_MAX || argv == 0) {
        return -TABOS_EINVAL;
    }

    int status = TABOS_ELF_EXEC_PENDING;
    while (status == TABOS_ELF_EXEC_PENDING) {
        status = api->exec(path, (uint32_t) argc, argv);
        if (status == TABOS_ELF_EXEC_PENDING) {
            api->yield();
        }
    }
    return status;
}

int tabos_spawn(const char* path, int argc, const char* const argv[])
{
    const tabos_elf_api_t* api = tabos_runtime_api;
    if (api == NULL || api->spawn == NULL || api->yield == NULL || path == NULL || path[0] == '\0' || argc < 1 ||
        argc > TABOS_ELF_ARG_MAX || argv == NULL) {
        return -TABOS_EINVAL;
    }
    int result;
    do {
        result = api->spawn(path, (uint32_t) argc, argv);
        if (result == TABOS_ELF_EXEC_PENDING) {
            api->yield();
        }
    } while (result == TABOS_ELF_EXEC_PENDING);
    return result;
}

int tabos_waitpid(int pid, int* status)
{
    const tabos_elf_api_t* api = tabos_runtime_api;
    if (api == NULL || api->waitpid == NULL || api->yield == NULL || pid <= 0) {
        return -TABOS_EINVAL;
    }
    int copied_status = 0;
    int result;
    do {
        result = api->waitpid(pid, &copied_status);
        if (result == TABOS_ELF_EXEC_PENDING) {
            api->yield();
        }
    } while (result == TABOS_ELF_EXEC_PENDING);
    if (result > 0 && status != NULL) {
        *status = copied_status;
    }
    return result;
}

int execve(const char* path, char* const argv[], char* const envp[])
{
    (void) envp;
    int argc = 0;
    while (argv != NULL && argv[argc] != NULL) {
        ++argc;
    }
    return tabos_exec(path, argc, (const char* const*) argv);
}

int waitpid(int pid, int* status, int options)
{
    if (options != 0) {
        errno = TABOS_EINVAL;
        return -1;
    }
    const int result = tabos_waitpid(pid, status);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return result;
}
