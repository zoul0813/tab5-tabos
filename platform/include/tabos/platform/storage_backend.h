#ifndef TABOS_PLATFORM_STORAGE_BACKEND_H
#define TABOS_PLATFORM_STORAGE_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

size_t storage_backend_drive_count(void);
bool storage_backend_mount(size_t index, char* letter, char* root, size_t root_size, bool* removable,
                           const char** name);
void storage_backend_unmount(char letter);
bool storage_backend_info(char letter, uint64_t* total_bytes, uint64_t* free_bytes);

#endif
