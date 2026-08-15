#ifndef TABOS_PLATFORM_STORAGE_BACKEND_H
#define TABOS_PLATFORM_STORAGE_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool storage_backend_mount(char *root, size_t root_size,
                               bool *removable, const char **name);
void storage_backend_unmount(void);
bool storage_backend_info(uint64_t *total_bytes, uint64_t *free_bytes);

#endif
