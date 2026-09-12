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

static int query_call(const char* path, tabos_program_info_t* info)
{
    info->flags      = TABOS_PROGRAM_GUI;
    info->heap_bytes = 4096U;
    return strcmp(path, "client") == 0 ? 0 : -TABOS_ENOENT;
}

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
    const char* const argv[]  = {"client", NULL};
    tabos_program_info_t info = {0};
    assert(tabos_program_query("client", &info) == -1 && errno == ENOSYS);
    assert(tabos_spawn("T:/bin/client", 1, argv) == -TABOS_EINVAL);
    assert(tabos_waitpid(17, NULL) == -TABOS_EINVAL);
    tabos_elf_api_t api = {.spawn = spawn_call, .waitpid = wait_call, .yield = yield_call};
    tabos_runtime_api   = &api;
    api.program_query   = query_call;
    assert(tabos_program_query(NULL, &info) == -1 && errno == EINVAL);
    assert(tabos_program_query("client", &info) == 0 && info.flags == TABOS_PROGRAM_GUI && info.heap_bytes == 4096U);
    info.flags = 99U;
    assert(tabos_program_query("missing", &info) == -1 && errno == TABOS_ENOENT && info.flags == 99U);
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
