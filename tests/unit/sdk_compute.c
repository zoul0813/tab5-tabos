#include <tabos/compute.h>
#include <tabos/internal/elf_api.h>
#include <tabos/filesystem.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>

static uint32_t payload;
static int reply;
static void calculate(void* data)
{
    (void) data;
}
static int submit(uintptr_t address, void* data, uint32_t bytes)
{
    tabos_compute_fn function = calculate;
    uintptr_t expected        = 0U;
    memcpy(&expected, &function, sizeof(function));
    assert(address == expected && data == &payload && bytes == sizeof(payload));
    return reply;
}
static int completion(void)
{
    return reply;
}
static tabos_elf_api_t api = {.compute_submit = submit, .compute_poll = completion, .compute_wait = completion};
const tabos_elf_api_t* tabos_runtime_api = &api;
int main(void)
{
    const int replies[] = {0, 1, -TABOS_EBUSY, -TABOS_EPERM, -TABOS_ECANCELED};
    for (size_t i = 0U; i < sizeof(replies) / sizeof(replies[0]); ++i) {
        reply = replies[i];
        assert(tabos_compute_submit(calculate, &payload, sizeof(payload)) == reply);
        assert(tabos_compute_poll() == reply && tabos_compute_wait() == reply);
    }
    api = (tabos_elf_api_t) {0};
    assert(tabos_compute_submit(calculate, &payload, sizeof(payload)) == -TABOS_ENOTSUP);
    assert(tabos_compute_poll() == -TABOS_ENOTSUP && tabos_compute_wait() == -TABOS_ENOTSUP);
    tabos_runtime_api = NULL;
    assert(tabos_compute_submit(calculate, &payload, sizeof(payload)) == -TABOS_ENOTSUP);
    assert(tabos_compute_poll() == -TABOS_ENOTSUP && tabos_compute_wait() == -TABOS_ENOTSUP);
    return 0;
}
