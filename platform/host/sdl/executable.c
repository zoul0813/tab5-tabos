#include <tabos/platform/platform.h>

#include <stdlib.h>

void *tab_platform_executable_alloc(size_t size)
{
    return malloc(size);
}

void *tab_platform_executable_prepare(void *memory, size_t size)
{
    if (memory == NULL || size == 0U) return NULL;
    __builtin___clear_cache((char *)memory, (char *)memory + size);
    return memory;
}

const void *tab_platform_executable_data_pointer(const void *memory)
{
    return memory;
}

void tab_platform_executable_free(void *memory)
{
    free(memory);
}

bool tab_platform_can_execute_riscv32(void)
{
    return false;
}
