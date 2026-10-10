#include <tabos/compute.h>
#include <tabos/internal/elf_api.h>
#include <tabos/filesystem.h>
#include <string.h>

extern const tabos_elf_api_t* tabos_runtime_api;

int tabos_compute_submit(tabos_compute_fn callback, void* data, uint32_t bytes)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->compute_submit == NULL) {
        return -TABOS_ENOTSUP;
    }
    uintptr_t address = 0U;
    _Static_assert(sizeof(address) == sizeof(callback), "compute callback pointer size");
    memcpy(&address, &callback, sizeof(address));
    return tabos_runtime_api->compute_submit(address, data, bytes);
}

int tabos_compute_wait(void)
{
    return tabos_runtime_api != NULL && tabos_runtime_api->compute_wait != NULL ? tabos_runtime_api->compute_wait() :
                                                                                  -TABOS_ENOTSUP;
}

int tabos_compute_poll(void)
{
    return tabos_runtime_api != NULL && tabos_runtime_api->compute_poll != NULL ? tabos_runtime_api->compute_poll() :
                                                                                  -TABOS_ENOTSUP;
}
