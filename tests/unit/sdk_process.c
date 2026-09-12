#include <tabos/process.h>
#include <tabos/internal/elf_api.h>
#include <tabos/posix_compat.h>
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>

const tabos_elf_api_t* tabos_runtime_api;
static unsigned int yields;
static unsigned int spawns;
static unsigned int waits;
static int next_pid = 17;
static bool reaped;

static void yield_call(void)
{
    ++yields;
}

static int spawn_call(const char* path, uint32_t argc, const char* const* argv)
{
    assert(strcmp(path, "T:/bin/client") == 0 && argc == 1U && strcmp(argv[0], "client") == 0);
    ++spawns;
    if ((spawns % 2U) != 0U) {
        return TABOS_ELF_EXEC_PENDING;
    }
    return next_pid++;
}

static int wait_call(int pid, int* status)
{
    ++waits;
    if (pid != 17 || reaped) {
        return -TABOS_ECHILD;
    }
    if (waits == 1U) {
        return TABOS_ELF_EXEC_PENDING;
    }
    *status = -7;
    reaped  = true;
    return pid;
}

int main(void)
{
    const char* const argv[] = {"client", NULL};
    assert(tabos_spawn("T:/bin/client", 1, argv) == -TABOS_EINVAL);
    assert(tabos_waitpid(17, NULL) == -TABOS_EINVAL);
    tabos_elf_api_t api = {.spawn = spawn_call, .waitpid = wait_call, .yield = yield_call};
    tabos_runtime_api   = &api;
    assert(tabos_spawn(NULL, 1, argv) == -TABOS_EINVAL);
    assert(tabos_spawn("", 1, argv) == -TABOS_EINVAL);
    assert(tabos_spawn("T:/bin/client", -1, argv) == -TABOS_EINVAL);
    assert(tabos_spawn("T:/bin/client", 17, argv) == -TABOS_EINVAL);
    assert(tabos_spawn("T:/bin/client", 1, NULL) == -TABOS_EINVAL);
    assert(spawns == 0U);
    assert(tabos_spawn("T:/bin/client", 1, argv) == 17);
    assert(tabos_spawn("T:/bin/client", 1, argv) == 18);
    assert(yields == 2U);
    int status = 99;
    assert(tabos_waitpid(17, &status) == 17 && status == -7 && yields == 3U);
    assert(tabos_waitpid(17, &status) == -TABOS_ECHILD && status == -7);
    assert(tabos_waitpid(18, NULL) == -TABOS_ECHILD);
    assert(tabos_waitpid(0, NULL) == -TABOS_EINVAL);
    assert(waitpid(17, &status, 1) == -1 && errno == TABOS_EINVAL);
    assert(waitpid(17, &status, 0) == -1 && errno == TABOS_ECHILD);
    api.waitpid = NULL;
    assert(tabos_waitpid(17, &status) == -TABOS_EINVAL);
    return 0;
}
