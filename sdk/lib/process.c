#include <tabos/process.h>

#include <tabos/internal/elf_api.h>
#include <tabos/posix_compat.h>
#include <errno.h>
#include <sched.h>

static const char *pending_path;
static int pending_argc;
static const char *const *pending_argv;
static int pending_pid;

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

int tabos_spawn(const char *path, int argc, const char *const argv[])
{
    if (pending_path != NULL) return -TABOS_EBUSY;
    const int result = tabos_runtime_api->exec(path, (uint32_t)argc, argv);
    if (result < 0 && result != TABOS_ELF_EXEC_PENDING) return result;
    pending_path = path;
    pending_argc = argc;
    pending_argv = argv;
    pending_pid = 1;
    return pending_pid;
}

int tabos_waitpid(int pid, int *status)
{
    if (pending_path == NULL || pid != pending_pid) return -TABOS_ECHILD;
    int result;
    do {
        result = tabos_runtime_api->exec(pending_path, (uint32_t)pending_argc, pending_argv);
        if (result == TABOS_ELF_EXEC_PENDING) (void)sched_yield();
    } while (result == TABOS_ELF_EXEC_PENDING);
    pending_path = NULL;
    if (status != NULL) *status = result;
    return pid;
}

int execve(const char *path, char *const argv[], char *const envp[])
{
    (void)envp;
    int argc = 0;
    while (argv != NULL && argv[argc] != NULL) ++argc;
    return tabos_exec(path, argc, (const char *const *)argv);
}

int waitpid(int pid, int *status, int options)
{
    if (options != 0) { errno = TABOS_EINVAL; return -1; }
    const int result = tabos_waitpid(pid, status);
    if (result < 0) { errno = -result; return -1; }
    return result;
}
